#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "gloomy/data/columnar_table.h"

namespace gloomy::data {
namespace {

auto MarketColumns() -> std::vector<ColumnInfo> {
  ColumnInfo station{
      .name = "station", .title = "Station", .type = ColumnType::kText};
  ColumnInfo price{
      .name = "price", .title = "Price", .type = ColumnType::kDouble};
  price.unit = " Cr";
  ColumnInfo stock{
      .name = "stock", .title = "Stock", .type = ColumnType::kInt64};
  ColumnInfo kind{.name = "kind", .title = "Kind", .type = ColumnType::kEnum};
  kind.enum_labels = {"Mine", "Smelter", "Farm"};
  ColumnInfo docked{
      .name = "docked", .title = "Docked", .type = ColumnType::kBool};
  return {station, price, stock, kind, docked};
}

TEST(FormatTest, IntegersGroupThousands) {
  std::string s;
  FormatInt(1234567, true, s);
  EXPECT_EQ(s, "1,234,567");
  s.clear();
  FormatInt(-1000, true, s);
  EXPECT_EQ(s, "-1,000");
  s.clear();
  FormatInt(999, true, s);
  EXPECT_EQ(s, "999");
  s.clear();
  FormatInt(1234567, false, s);
  EXPECT_EQ(s, "1234567");
}

TEST(FormatTest, DoublesRoundAndGroup) {
  std::string s;
  FormatDouble(12345.678, 2, true, s);
  EXPECT_EQ(s, "12,345.68");
  s.clear();
  FormatDouble(-0.5, 0, true, s);
  EXPECT_EQ(s, "-0");
  s.clear();
  FormatDouble(1500.0, 1, false, s);
  EXPECT_EQ(s, "1500.0");
}

TEST(ColumnarTableTest, StoresTypedColumnsAndFormats) {
  ColumnarTable t(MarketColumns());
  t.AddRow({"Mine 4", 12.5, std::int64_t{1200}, std::int64_t{0}, true});
  t.AddRow({"Smelter 2", 1234.5, std::int64_t{-3}, std::int64_t{1}, false});
  EXPECT_EQ(t.RowCount(), 2U);
  EXPECT_EQ(t.GetText(0, 1), "Smelter 2");
  EXPECT_DOUBLE_EQ(t.GetDouble(1, 0), 12.5);
  EXPECT_EQ(t.GetInt64(2, 1), -3);
  std::string s;
  t.FormatCell(1, 1, s);
  EXPECT_EQ(s, "1,234.50 Cr");
  t.FormatCell(0, 2, s);
  EXPECT_EQ(s, "1,200");
  t.FormatCell(1, 3, s);
  EXPECT_EQ(s, "Smelter");
  t.FormatCell(0, 4, s);
  EXPECT_EQ(s, "yes");
  EXPECT_EQ(t.FindColumn("stock"), 2);
  EXPECT_EQ(t.FindColumn("nope"), -1);
}

TEST(ColumnarTableTest, SettersBumpVersion) {
  ColumnarTable t(MarketColumns());
  t.AddRow({"A", 1.0, std::int64_t{1}, std::int64_t{0}, false});
  const auto v = t.Version();
  t.SetDouble(1, 0, 2.0);
  EXPECT_GT(t.Version(), v);
  t.SetText(0, 0, "Renamed");
  EXPECT_EQ(t.GetText(0, 0), "Renamed");
}

TEST(ColumnarTableTest, NaNFormatsBlank) {
  ColumnarTable t(MarketColumns());
  t.AddRow({"A",
            std::numeric_limits<double>::quiet_NaN(),
            std::int64_t{1},
            std::int64_t{0},
            false});
  std::string s = "x";
  t.FormatCell(0, 1, s);
  EXPECT_EQ(s, "");
}

TEST(FunctionTableTest, AdaptsStructureOfArrays) {
  // Like X-conomy's World tables.
  std::vector<std::string> names{"Argon Prime", "Home of Light"};
  std::vector<std::int64_t> prices{250, 300};
  std::uint64_t tick = 7;
  FunctionTable t([&] { return static_cast<RowIndex>(names.size()); },
                  [&] { return tick; });
  t.AddText({.name = "name", .title = "Sector"},
            [&](RowIndex r) -> std::string_view { return names[r]; });
  t.AddInt64({.name = "price", .title = "Price"},
             [&](RowIndex r) { return prices[r]; });
  t.SetRowKey([](RowIndex r) { return 1000 + r; });
  t.SetExplain([](RowIndex, int col, Explanation &e) {
    if (col != 1) {
      return false;
    }
    e.reason = "ore shortage";
    return true;
  });
  EXPECT_EQ(t.RowCount(), 2U);
  EXPECT_EQ(t.Version(), 7U);
  EXPECT_EQ(t.GetText(0, 1), "Home of Light");
  EXPECT_DOUBLE_EQ(t.GetDouble(1, 1), 300.0);
  EXPECT_EQ(t.RowKey(1), 1001U);
  Explanation e;
  EXPECT_TRUE(t.Explain(0, 1, e));
  EXPECT_EQ(e.reason, "ore shortage");
  EXPECT_FALSE(t.Explain(0, 0, e));
  tick = 8;
  EXPECT_EQ(t.Version(), 8U);
}

}  // namespace
}  // namespace gloomy::data
