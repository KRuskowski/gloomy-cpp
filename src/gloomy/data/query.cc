#include "gloomy/data/query.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

#include "gloomy/core/id.h"
#include "gloomy/core/parallel.h"
#include "gloomy/core/utf8.h"

namespace gloomy::data {

namespace {

constexpr std::size_t kChunk = 4096;
constexpr std::size_t kMinParallelSort = 100000;

auto Cancelled(const std::atomic<bool> *cancel) -> bool {
  return cancel != nullptr && cancel->load(std::memory_order_relaxed);
}

auto IsTextOp(FilterOp op) -> bool {
  return op == FilterOp::kContains || op == FilterOp::kNotContains ||
         op == FilterOp::kStartsWith || op == FilterOp::kEndsWith ||
         op == FilterOp::kTextEquals;
}

// Pre-extracted sort key of one column, indexed by position in the row
// list being sorted.
struct KeyColumn {
  ColumnType type = ColumnType::kInt64;
  bool descending = false;
  std::vector<std::int64_t> ints;
  std::vector<double> doubles;
  // Text: folded strings in an arena plus an 8-byte big-endian prefix for
  // fast comparison.
  std::string arena;
  std::vector<std::uint64_t> prefix;
  std::vector<std::uint32_t> offset;
  std::vector<std::uint32_t> length;
};

auto Prefix(std::string_view s) -> std::uint64_t {
  std::uint64_t p = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    p <<= 8U;
    if (i < s.size()) {
      p |= static_cast<unsigned char>(s[i]);
    }
  }
  return p;
}

void Extract(const TableSource &source, const SortKey &key,
             const std::vector<RowIndex> &rows, KeyColumn &out) {
  const ColumnInfo &info = source.Columns()[key.column];
  out.type = info.type;
  out.descending = key.direction == SortDirection::kDescending;
  const std::size_t n = rows.size();
  switch (info.type) {
    case ColumnType::kDouble:
      out.doubles.resize(n);
      source.ReadDouble(key.column, rows, out.doubles);
      break;
    case ColumnType::kText: {
      out.prefix.resize(n);
      out.offset.resize(n);
      out.length.resize(n);
      std::string folded;
      for (std::size_t i = 0; i < n; ++i) {
        FoldCaseUtf8(source.GetText(key.column, rows[i]), folded);
        out.prefix[i] = Prefix(folded);
        out.offset[i] = static_cast<std::uint32_t>(out.arena.size());
        out.length[i] = static_cast<std::uint32_t>(folded.size());
        out.arena += folded;
      }
      break;
    }
    default:
      out.ints.resize(n);
      source.ReadInt64(key.column, rows, out.ints);
      break;
  }
}

// Three-way comparison of positions a and b on one key; blanks (NaN) last
// in either direction.
auto Compare(const KeyColumn &k, std::uint32_t a, std::uint32_t b) -> int {
  int c = 0;
  switch (k.type) {
    case ColumnType::kDouble: {
      const double x = k.doubles[a];
      const double y = k.doubles[b];
      const bool xn = std::isnan(x);
      const bool yn = std::isnan(y);
      if (xn || yn) {
        return xn == yn ? 0 : (xn ? 1 : -1);
      }
      c = x < y ? -1 : (x > y ? 1 : 0);
      break;
    }
    case ColumnType::kText: {
      if (k.prefix[a] != k.prefix[b]) {
        c = k.prefix[a] < k.prefix[b] ? -1 : 1;
        break;
      }
      const std::string_view x(k.arena.data() + k.offset[a], k.length[a]);
      const std::string_view y(k.arena.data() + k.offset[b], k.length[b]);
      c = x.compare(y);
      c = c < 0 ? -1 : (c > 0 ? 1 : 0);
      break;
    }
    default:
      c = k.ints[a] < k.ints[b] ? -1 : (k.ints[a] > k.ints[b] ? 1 : 0);
      break;
  }
  return k.descending ? -c : c;
}

}  // namespace

auto IsNarrowing(const Query &prev, const Query &next) -> bool {
  if (prev.sort != next.sort || prev.search_columns != next.search_columns ||
      prev.group_by != next.group_by) {
    return false;
  }
  for (const Filter &f : prev.filters) {
    if (std::find(next.filters.begin(), next.filters.end(), f) ==
        next.filters.end()) {
      return false;
    }
  }
  return TermsNarrow(ParseTerms(prev.search), ParseTerms(next.search));
}

auto EffectiveSort(const Query &query) -> std::vector<SortKey> {
  std::vector<SortKey> keys;
  for (const int col : query.group_by) {
    SortKey key{col, SortDirection::kAscending};
    for (const SortKey &k : query.sort) {
      if (k.column == col) {
        key.direction = k.direction;
      }
    }
    keys.push_back(key);
  }
  for (const SortKey &k : query.sort) {
    const bool grouped =
        std::find(query.group_by.begin(), query.group_by.end(), k.column) !=
        query.group_by.end();
    if (!grouped) {
      keys.push_back(k);
    }
  }
  return keys;
}

namespace {

void Summarise(const TableSource &source, const Query &query,
               const std::vector<RowIndex> &rows, RowGroup &group) {
  group.values.clear();
  for (const Aggregate &a : query.aggregates) {
    double sum = 0.0;
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    std::uint32_t n = 0;
    for (std::uint32_t i = group.first; i < group.first + group.count; ++i) {
      const double v = source.GetDouble(a.column, rows[i]);
      if (std::isnan(v)) {
        continue;
      }
      sum += v;
      lo = std::min(lo, v);
      hi = std::max(hi, v);
      ++n;
    }
    double value = std::numeric_limits<double>::quiet_NaN();
    switch (a.op) {
      case AggregateOp::kCount:
        value = group.count;
        break;
      case AggregateOp::kSum:
        value = n > 0 ? sum : value;
        break;
      case AggregateOp::kMin:
        value = n > 0 ? lo : value;
        break;
      case AggregateOp::kMax:
        value = n > 0 ? hi : value;
        break;
      case AggregateOp::kMean:
        value = n > 0 ? sum / n : value;
        break;
    }
    group.values.push_back(value);
  }
}

auto GroupLevel(const TableSource &source, const Query &query, RowView &view,
                std::size_t level, std::uint32_t begin, std::uint32_t end,
                std::int32_t parent, std::uint64_t path,
                const std::atomic<bool> *cancel) -> bool {
  const int col = query.group_by[level];
  std::string key;
  std::string next;
  std::uint32_t i = begin;
  while (i < end) {
    if (Cancelled(cancel)) {
      return false;
    }
    source.FormatCell(view.rows[i], col, key);
    std::uint32_t j = i + 1;
    while (j < end) {
      source.FormatCell(view.rows[j], col, next);
      if (next != key) {
        break;
      }
      ++j;
    }
    RowGroup group;
    group.first = i;
    group.count = j - i;
    group.level = static_cast<std::uint16_t>(level);
    group.parent = parent;
    group.id = HashString(key, path);
    group.key = key;
    Summarise(source, query, view.rows, group);
    const auto index = static_cast<std::int32_t>(view.groups.size());
    const std::uint64_t id = group.id;
    view.groups.push_back(std::move(group));
    if (level + 1 < query.group_by.size() &&
        !GroupLevel(source, query, view, level + 1, i, j, index, id, cancel)) {
      return false;
    }
    i = j;
  }
  return true;
}

}  // namespace

auto BuildGroups(const TableSource &source, const Query &query, RowView &view,
                 const std::atomic<bool> *cancel) -> bool {
  view.groups.clear();
  if (query.group_by.empty()) {
    return true;
  }
  return GroupLevel(source,
                    query,
                    view,
                    0,
                    0,
                    static_cast<std::uint32_t>(view.rows.size()),
                    -1,
                    0x67726F7570ULL,
                    cancel);
}

auto SameSelection(const Query &prev, const Query &next) -> bool {
  return prev.filters == next.filters && prev.search == next.search &&
         prev.search_columns == next.search_columns;
}

auto Passes(const TableSource &source, const Filter &f, RowIndex row) -> bool {
  const ColumnInfo &info = source.Columns()[f.column];
  if (f.op == FilterOp::kInSet) {
    const std::int64_t v = source.GetInt64(f.column, row);
    return std::find(f.set.begin(), f.set.end(), v) != f.set.end();
  }
  if (IsTextOp(f.op) || f.op == FilterOp::kBlank ||
      f.op == FilterOp::kNotBlank) {
    std::string cell;
    if (info.type == ColumnType::kText) {
      cell.assign(source.GetText(f.column, row));
    } else if (info.type == ColumnType::kDouble &&
               std::isnan(source.GetDouble(f.column, row))) {
      cell.clear();
    } else {
      source.FormatCell(row, f.column, cell);
    }
    if (f.op == FilterOp::kBlank) {
      return cell.empty();
    }
    if (f.op == FilterOp::kNotBlank) {
      return !cell.empty();
    }
    std::string hay;
    std::string needle;
    FoldCaseUtf8(cell, hay);
    FoldCaseUtf8(f.text, needle);
    switch (f.op) {
      case FilterOp::kContains:
        return hay.find(needle) != std::string::npos;
      case FilterOp::kNotContains:
        return hay.find(needle) == std::string::npos;
      case FilterOp::kStartsWith:
        return hay.starts_with(needle);
      case FilterOp::kEndsWith:
        return hay.ends_with(needle);
      default:
        return hay == needle;
    }
  }
  const double v = source.GetDouble(f.column, row);
  if (std::isnan(v)) {
    return false;
  }
  switch (f.op) {
    case FilterOp::kEqual:
      return v == f.a;
    case FilterOp::kNotEqual:
      return v != f.a;
    case FilterOp::kLess:
      return v < f.a;
    case FilterOp::kLessEqual:
      return v <= f.a;
    case FilterOp::kGreater:
      return v > f.a;
    case FilterOp::kGreaterEqual:
      return v >= f.a;
    case FilterOp::kBetween:
      return v >= f.a && v <= f.b;
    default:
      return true;
  }
}

auto SortRows(const TableSource &source, std::span<const SortKey> keys,
              std::vector<RowIndex> &rows, const std::atomic<bool> *cancel)
    -> bool {
  if (keys.empty() || rows.size() < 2) {
    return true;
  }
  std::vector<KeyColumn> columns(keys.size());
  for (std::size_t k = 0; k < keys.size(); ++k) {
    if (Cancelled(cancel)) {
      return false;
    }
    Extract(source, keys[k], rows, columns[k]);
  }
  std::vector<std::uint32_t> order(rows.size());
  std::iota(order.begin(), order.end(), 0U);
  auto less = [&](std::uint32_t a, std::uint32_t b) {
    for (const KeyColumn &k : columns) {
      const int c = Compare(k, a, b);
      if (c != 0) {
        return c < 0;
      }
    }
    return rows[a] < rows[b];
  };
  // Sort chunks in parallel, then merge them pairwise.
  const int chunks = ChunkCount(order.size(), kMinParallelSort);
  const std::size_t per = (order.size() + chunks - 1) / chunks;
  ParallelChunks(order.size(),
                 kMinParallelSort,
                 [&](std::size_t begin, std::size_t end, int) {
                   std::sort(order.begin() + static_cast<std::ptrdiff_t>(begin),
                             order.begin() + static_cast<std::ptrdiff_t>(end),
                             less);
                 });
  if (Cancelled(cancel)) {
    return false;
  }
  for (std::size_t width = per; width < order.size(); width *= 2) {
    for (std::size_t lo = 0; lo + width < order.size(); lo += 2 * width) {
      const std::size_t mid = lo + width;
      const std::size_t hi = std::min(order.size(), lo + 2 * width);
      std::inplace_merge(order.begin() + static_cast<std::ptrdiff_t>(lo),
                         order.begin() + static_cast<std::ptrdiff_t>(mid),
                         order.begin() + static_cast<std::ptrdiff_t>(hi),
                         less);
    }
  }
  std::vector<RowIndex> sorted(rows.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    sorted[i] = rows[order[i]];
  }
  rows.swap(sorted);
  return true;
}

auto Execute(const TableSource &source, const Query &query, const RowView *prev,
             const SearchCorpus *corpus, const std::atomic<bool> *cancel)
    -> std::optional<RowView> {
  RowView result;
  result.query = query;
  result.version = source.Version();
  const bool same_data = prev != nullptr && prev->version == result.version;

  if (same_data && prev->query == query) {
    result.rows = prev->rows;
    result.path = RowView::Path::kCached;
    return result;
  }
  if (same_data && SameSelection(prev->query, query)) {
    result.rows = prev->rows;
    result.path = RowView::Path::kResorted;
    if (!SortRows(source, EffectiveSort(query), result.rows, cancel) ||
        !BuildGroups(source, query, result, cancel)) {
      return std::nullopt;
    }
    return result;
  }

  bool sorted = false;
  if (same_data && IsNarrowing(prev->query, query)) {
    result.rows = prev->rows;
    result.path = RowView::Path::kNarrowed;
    sorted = true;
  } else {
    result.rows.resize(source.RowCount());
    std::iota(result.rows.begin(), result.rows.end(), RowIndex{0});
  }

  // Filters, cheapest first: numeric and set filters before text ones.
  std::vector<const Filter *> filters;
  for (const Filter &f : query.filters) {
    filters.push_back(&f);
  }
  std::stable_sort(
      filters.begin(), filters.end(), [](const Filter *a, const Filter *b) {
        return !IsTextOp(a->op) && IsTextOp(b->op);
      });
  for (const Filter *f : filters) {
    std::size_t w = 0;
    for (std::size_t i = 0; i < result.rows.size(); ++i) {
      if (i % kChunk == 0 && Cancelled(cancel)) {
        return std::nullopt;
      }
      if (Passes(source, *f, result.rows[i])) {
        result.rows[w++] = result.rows[i];
      }
    }
    result.rows.resize(w);
  }

  const std::vector<std::string> terms = ParseTerms(query.search);
  if (!terms.empty()) {
    if (corpus == nullptr) {
      return std::nullopt;
    }
    if (!corpus->Filter(result.rows, terms, cancel)) {
      return std::nullopt;
    }
  }

  if (!sorted && !SortRows(source, EffectiveSort(query), result.rows, cancel)) {
    return std::nullopt;
  }
  if (!BuildGroups(source, query, result, cancel)) {
    return std::nullopt;
  }
  return result;
}

}  // namespace gloomy::data
