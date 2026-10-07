#include "gloomy/data/table_view.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "gloomy/data/columnar_table.h"

namespace gloomy::data {
namespace {

auto MakeTable(int rows) -> std::shared_ptr<ColumnarTable> {
  ColumnInfo name{.name = "name", .title = "Name", .type = ColumnType::kText};
  ColumnInfo value{
      .name = "value", .title = "Value", .type = ColumnType::kInt64};
  auto t =
      std::make_shared<ColumnarTable>(std::vector<ColumnInfo>{name, value});
  t->Reserve(static_cast<RowIndex>(rows));
  for (int i = 0; i < rows; ++i) {
    const std::string n = "Station " + std::to_string(i);
    t->AddRow({std::string_view(n), std::int64_t{(i * 7919) % 1000}});
  }
  return t;
}

TEST(TableViewTest, SmallTablesUpdateSynchronously) {
  auto t = MakeTable(100);
  TableView view(t);
  EXPECT_TRUE(view.Update());
  EXPECT_EQ(view.rows().size(), 100U);
  Query q;
  // A quoted phrase: "Station 4" and "Station 40" to "Station 49".
  q.search = "\"station 4\"";
  view.SetQuery(q);
  EXPECT_TRUE(view.Update());
  EXPECT_FALSE(view.busy());
  EXPECT_EQ(view.rows().size(), 11U);
  EXPECT_EQ(view.terms(), (std::vector<std::string>{"station 4"}));
}

TEST(TableViewTest, LargeTablesRunInTheBackground) {
  auto t = MakeTable(200000);
  TableView view(t);
  view.Finish();
  ASSERT_EQ(view.rows().size(), 200000U);
  Query q;
  q.sort = {{1, SortDirection::kDescending}};
  q.search = "1999";
  view.SetQuery(q);
  view.Update();
  // The old result stays visible while the query runs.
  EXPECT_EQ(view.rows().size(), 200000U);
  view.Finish();
  EXPECT_FALSE(view.busy());
  ASSERT_FALSE(view.rows().empty());
  for (std::size_t i = 1; i < view.rows().size(); ++i) {
    EXPECT_GE(t->GetInt64(1, view.rows()[i - 1]),
              t->GetInt64(1, view.rows()[i]));
  }
}

TEST(TableViewTest, NewerQueryWins) {
  auto t = MakeTable(200000);
  TableView view(t);
  view.Finish();
  Query a;
  a.search = "1";
  view.SetQuery(a);
  view.Update();
  Query b;
  b.search = "\"station 12345\"";
  view.SetQuery(b);
  view.Update();
  view.Finish();
  EXPECT_EQ(view.view().query, b);
  EXPECT_EQ(view.rows().size(), 11U);
}

TEST(TableViewTest, TypingNarrowsAndBackspaceIsCached) {
  auto t = MakeTable(1000);
  TableView view(t);
  view.Update();
  Query q;
  q.search = "1";
  view.SetQuery(q);
  view.Update();
  q.search = "12";
  view.SetQuery(q);
  view.Update();
  EXPECT_EQ(view.view().path, RowView::Path::kNarrowed);
  q.search = "1";
  view.SetQuery(q);
  view.Update();
  EXPECT_EQ(view.view().path, RowView::Path::kCached);
}

TEST(TableViewTest, RefreshesWhenTheSourceChanges) {
  auto t = MakeTable(10);
  TableView view(t);
  view.SetRefreshInterval(0.25);
  view.Update(0.0);
  t->AddRow({"Station new", std::int64_t{1}});
  EXPECT_FALSE(view.Update(0.1)) << "throttled";
  EXPECT_TRUE(view.Update(0.3));
  EXPECT_EQ(view.rows().size(), 11U);
}

TEST(TableViewTest, PositionOfKey) {
  auto t = MakeTable(10);
  TableView view(t);
  Query q;
  q.sort = {{1, SortDirection::kAscending}};
  view.SetQuery(q);
  view.Update();
  const std::int64_t pos = view.PositionOfKey(3);
  ASSERT_GE(pos, 0);
  EXPECT_EQ(view.rows()[static_cast<std::size_t>(pos)], 3U);
  EXPECT_EQ(view.PositionOfKey(99), -1);
}

}  // namespace
}  // namespace gloomy::data
