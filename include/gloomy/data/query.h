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

/// How group rows summarise a column.
enum class AggregateOp : std::uint8_t { kCount, kSum, kMin, kMax, kMean };

struct Aggregate {
  int column = 0;
  AggregateOp op = AggregateOp::kSum;

  friend auto operator==(const Aggregate &, const Aggregate &)
      -> bool = default;
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
  /// Row grouping, outermost first. Grouped rows sort by these columns
  /// first (in the direction of a matching sort key, else ascending).
  std::vector<int> group_by;
  /// Values computed for every group.
  std::vector<Aggregate> aggregates;

  friend auto operator==(const Query &, const Query &) -> bool = default;
};

/// A group of consecutive rows in a grouped RowView.
struct RowGroup {
  /// Range in RowView::rows.
  std::uint32_t first = 0;
  std::uint32_t count = 0;
  /// Nesting level; 0 is the outermost grouping.
  std::uint16_t level = 0;
  /// Index of the enclosing group, -1 at level 0.
  std::int32_t parent = -1;
  /// Stable identity (hash of the key path), e.g. to remember which
  /// groups are collapsed across queries.
  std::uint64_t id = 0;
  /// Formatted value of the group column.
  std::string key;
  /// One value per Query::aggregates; NaN if there is nothing to compute.
  std::vector<double> values;
};

/// The result of a query: matching source rows in display order. 4 bytes
/// per row; the data itself is never copied.
struct RowView {
  std::vector<RowIndex> rows;
  /// With grouping: the groups in pre-order (each followed by its
  /// subgroups).
  std::vector<RowGroup> groups;
  Query query;
  /// Source version the rows were computed from.
  std::uint64_t version = 0;
  /// How the result was obtained, for tests and diagnostics.
  /// Source row count the rows were computed from (append-only sources
  /// extend from here).
  RowIndex source_rows = 0;
  enum class Path : std::uint8_t {
    kFull,
    kNarrowed,
    kResorted,
    kCached,
    kAppended,
  };
  Path path = Path::kFull;
};

/// True if `next` selects a subset of what `prev` selects on the same data
/// with the same order, so it may run on the previous result only.
auto IsNarrowing(const Query &prev, const Query &next) -> bool;

/// True if `next` selects exactly the rows of `prev` (only the order
/// differs).
auto SameSelection(const Query &prev, const Query &next) -> bool;

/// The sort keys a query really sorts by: group columns first, then the
/// query's own keys.
auto EffectiveSort(const Query &query) -> std::vector<SortKey>;

/// Builds RowView::groups for rows sorted by EffectiveSort().
/// @return False if cancelled.
auto BuildGroups(const TableSource &source, const Query &query, RowView &view,
                 const std::atomic<bool> *cancel = nullptr) -> bool;

/// True if row `row` passes `filter`.
auto Passes(const TableSource &source, const Filter &filter, RowIndex row)
    -> bool;

/// Sorts `rows` by `keys`; ties by row index, so results are deterministic.
/// Large inputs are sorted in parallel.
/// @return False if cancelled.
auto SortRows(const TableSource &source, std::span<const SortKey> keys,
              std::vector<RowIndex> &rows,
              const std::atomic<bool> *cancel = nullptr) -> bool;

/// Extends `prev` (same query) with the rows an append-only source added
/// since: they are filtered, searched, sorted and merged in. `corpus` must
/// cover them when the query searches.
/// @return std::nullopt if cancelled.
auto ExecuteAppend(const TableSource &source, const Query &query,
                   const RowView &prev, const SearchCorpus *corpus,
                   const std::atomic<bool> *cancel = nullptr)
    -> std::optional<RowView>;

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
