#ifndef GLOOMY_DATA_QUERY_H_
#define GLOOMY_DATA_QUERY_H_

#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "gloomy/data/search.h"
#include "gloomy/data/table_source.h"

namespace gloomy::data {

enum class SortDirection : std::uint8_t { kAscending, kDescending };

struct SortKey {
  int column = 0;
  SortDirection direction = SortDirection::kAscending;

  friend auto operator==(const SortKey &, const SortKey &) -> bool = default;
};

/// Typed filter operators. Numeric ones compare GetDouble(); text ones are
/// case-insensitive on the formatted cell; kInSet compares GetInt64()
/// (enums, bools).
enum class FilterOp : std::uint8_t {
  kEqual,
  kNotEqual,
  kLess,
  kLessEqual,
  kGreater,
  kGreaterEqual,
  /// a <= value <= b.
  kBetween,
  kContains,
  kNotContains,
  kStartsWith,
  kEndsWith,
  kTextEquals,
  kInSet,
  kBlank,
  kNotBlank,
};

/// A filter on one column.
struct Filter {
  int column = 0;
  FilterOp op = FilterOp::kContains;
  double a = 0.0;
  double b = 0.0;
  std::string text;
  std::vector<std::int64_t> set;

  friend auto operator==(const Filter &, const Filter &) -> bool = default;
};

/// What a view shows: a value type, saved in view presets
/// (docs/design.md section 10.2).
struct Query {
  /// Sort keys, most significant first. Ties keep source order.
  std::vector<SortKey> sort;
  /// All must match.
  std::vector<Filter> filters;
  /// Quick search; see ParseTerms().
  std::string search;
  /// Columns quick search looks at; empty means all searchable ones.
  std::vector<int> search_columns;

  friend auto operator==(const Query &, const Query &) -> bool = default;
};

/// The result of a query: matching source rows in display order. 4 bytes
/// per row; the data itself is never copied.
struct RowView {
  std::vector<RowIndex> rows;
  Query query;
  /// Source version the rows were computed from.
  std::uint64_t version = 0;
  /// How the result was obtained, for tests and diagnostics.
  enum class Path : std::uint8_t { kFull, kNarrowed, kResorted, kCached };
  Path path = Path::kFull;
};

/// True if `next` selects a subset of what `prev` selects on the same data
/// with the same order, so it may run on the previous result only.
auto IsNarrowing(const Query &prev, const Query &next) -> bool;

/// True if `next` selects exactly the rows of `prev` (only the order
/// differs).
auto SameSelection(const Query &prev, const Query &next) -> bool;

/// True if row `row` passes `filter`.
auto Passes(const TableSource &source, const Filter &filter, RowIndex row)
    -> bool;

/// Sorts `rows` by `keys`; ties by row index, so results are deterministic.
/// Large inputs are sorted in parallel.
/// @return False if cancelled.
auto SortRows(const TableSource &source, std::span<const SortKey> keys,
              std::vector<RowIndex> &rows,
              const std::atomic<bool> *cancel = nullptr) -> bool;

/// Runs `query` on `source`. With `prev` from the same source version, a
/// narrowing query runs on prev's rows and a re-sort only reorders them.
/// `corpus` is needed when the query has a search string.
/// @return std::nullopt if cancelled.
auto Execute(const TableSource &source, const Query &query, const RowView *prev,
             const SearchCorpus *corpus,
             const std::atomic<bool> *cancel = nullptr)
    -> std::optional<RowView>;

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_QUERY_H_
