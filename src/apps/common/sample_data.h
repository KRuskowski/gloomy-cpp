#ifndef GLOOMY_APPS_COMMON_SAMPLE_DATA_H_
#define GLOOMY_APPS_COMMON_SAMPLE_DATA_H_

#include <memory>
#include <string>

#include "gloomy/data/columnar_table.h"

namespace gloomy::apps {

/// Columns of the sample market table.
enum MarketColumn : int {
  kStation,
  kSector,
  kGood,
  kKind,
  kPrice,
  kStock,
  kAge,
  kOwner,
};

/// A deterministic X-conomy-like market table: one row per station and
/// good, with explained prices.
class SampleMarket : public data::ColumnarTable {
 public:
  explicit SampleMarket(int rows);

  auto Explain(data::RowIndex row, int col, data::Explanation &out) const
      -> bool override;

  /// Average price of the good named `good` (for the summary cards).
  auto AveragePrice(std::string_view good) const -> double;
};

/// A supply contract in the Markdown subset, `clauses` clauses long.
auto SampleContract(int clauses) -> std::string;

}  // namespace gloomy::apps

#endif  // GLOOMY_APPS_COMMON_SAMPLE_DATA_H_
