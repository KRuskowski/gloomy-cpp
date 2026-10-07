#ifndef GLOOMY_TESTS_TEST_SUPPORT_MARKET_TABLE_H_
#define GLOOMY_TESTS_TEST_SUPPORT_MARKET_TABLE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gloomy/data/columnar_table.h"

namespace gloomy::testing {

/// A deterministic market table like X-conomy's: station, sector, good,
/// kind (enum), price, stock, age of the report. Price cells explain
/// themselves.
class MarketTable : public data::ColumnarTable {
 public:
  explicit MarketTable(int rows) : data::ColumnarTable(MakeColumns()) {
    static const char *kSectors[] = {
        "Argon Prime", "Home of Light", "Red Light", "Ore Belt", "Cloudbase"};
    static const char *kGoods[] = {
        "Energy Cells", "Ore", "Silicon Wafers", "Food Rations", "Ship Parts"};
    Reserve(static_cast<data::RowIndex>(rows));
    std::uint64_t x = 12345;
    auto next = [&] {
      x = x * 6364136223846793005ULL + 1442695040888963407ULL;
      return static_cast<int>((x >> 33U) % 100000U);
    };
    for (int i = 0; i < rows; ++i) {
      const std::string station = "Station " + std::to_string(i);
      AddRow({std::string_view(station),
              std::string_view(kSectors[i % 5]),
              std::string_view(kGoods[(i / 5) % 5]),
              std::int64_t{(i / 3) % 3},
              10.0 + next() / 100.0,
              std::int64_t{next()},
              static_cast<double>(next() % 600)});
    }
  }

  auto Explain(data::RowIndex row, int col, data::Explanation &out) const
      -> bool override {
    if (col != 4) {
      return false;
    }
    out.reason = row % 2 == 0 ? "Shortage: stock below a day of demand"
                              : "Surplus: producers idle";
    out.breakdown = {{"Base", 10.0}, {"Premium", GetDouble(4, row) - 10.0}};
    return true;
  }

  // A short price history ending at the current price.
  auto GetSeries(int col, data::RowIndex row, std::vector<float> &out) const
      -> bool override {
    if (col != 4) {
      return false;
    }
    const auto price = static_cast<float>(GetDouble(4, row));
    std::uint32_t x = row * 2654435761U + 1U;
    for (int i = 0; i < 12; ++i) {
      x = x * 1664525U + 1013904223U;
      const float wobble = static_cast<float>(x % 1000) / 1000.0F - 0.5F;
      out.push_back(price * (1.0F + 0.08F * wobble + 0.01F * (i - 6)));
    }
    out.push_back(price);
    return true;
  }

  static auto MakeColumns() -> std::vector<data::ColumnInfo> {
    using data::ColumnInfo;
    using data::ColumnType;
    ColumnInfo kind{.name = "kind", .title = "Kind", .type = ColumnType::kEnum};
    kind.enum_labels = {"Mine", "Factory", "Trade"};
    ColumnInfo price{
        .name = "price", .title = "Price", .type = ColumnType::kDouble};
    price.unit = " Cr";
    price.explained = true;
    ColumnInfo age{.name = "age", .title = "Age", .type = ColumnType::kDouble};
    age.decimals = 0;
    age.unit = " s";
    return {{.name = "station", .title = "Station", .type = ColumnType::kText},
            {.name = "sector", .title = "Sector", .type = ColumnType::kText},
            {.name = "good", .title = "Good", .type = ColumnType::kText},
            kind,
            price,
            {.name = "stock", .title = "Stock", .type = ColumnType::kInt64},
            age};
  }
};

}  // namespace gloomy::testing

#endif  // GLOOMY_TESTS_TEST_SUPPORT_MARKET_TABLE_H_
