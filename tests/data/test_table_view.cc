#include "gloomy/data/table_view.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

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

/// A source whose rows can also go away (an app's table rebuilt from a
/// smaller world).
class ShrinkingSource : public TableSource {
 public:
  explicit ShrinkingSource(RowIndex rows) : rows_(rows) {}

  void SetRows(RowIndex rows) {
    rows_ = rows;
    ++version_;
  }

  auto RowCount() const -> RowIndex override { return rows_; }

  auto Columns() const -> std::span<const ColumnInfo> override {
    return columns_;
  }

  auto Version() const -> std::uint64_t override { return version_; }

  auto GetInt64(int, RowIndex row) const -> std::int64_t override {
    EXPECT_LT(row, rows_) << "read past the end";
    return row;
  }

 private:
  RowIndex rows_;
  std::uint64_t version_ = 1;
  std::vector<ColumnInfo> columns_ = {
      {.name = "n", .title = "N", .type = ColumnType::kInt64}};
};

TEST(TableViewTest, ASourceThatShrankIsQueriedAtOnce) {
  auto source = std::make_shared<ShrinkingSource>(20);
  TableView view(source);
  view.SetRefreshInterval(10.0);
  view.Update(0.0);
  ASSERT_EQ(view.rows().size(), 20U);
  source->SetRows(5);
  // Within the refresh interval, yet the rows must not point past the end.
  view.Update(0.1);
  ASSERT_EQ(view.rows().size(), 5U);
  for (const RowIndex r : view.rows()) {
    EXPECT_LT(r, source->RowCount());
  }
  // Growing waits for the interval as before.
  source->SetRows(8);
  EXPECT_FALSE(view.Update(0.2));
  EXPECT_EQ(view.rows().size(), 5U);
}

TEST(TableViewTest, SlowQueriesAreNotRestartedByRefreshes) {
  auto t = MakeTable(300000);
  TableView view(t);
  view.SetRefreshInterval(0.0);
  Query q;
  q.sort = {{0, SortDirection::kDescending}};
  view.SetQuery(q);
  // Frames far apart in simulated time, close in real time: every frame
  // is past the refresh interval.
  double time = 0.0;
  for (int i = 0; i < 20000 && (view.busy() || view.rows().empty()); ++i) {
    time += 1.0;
    view.Update(time);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_FALSE(view.busy());
  EXPECT_EQ(view.rows().size(), 300000U);
}

TEST(TableViewTest, AppendOnlySourcesExtendTheView) {
  auto t = MakeTable(1000);
  t->SetAppendOnly(true);
  TableView view(t);
  view.SetRefreshInterval(0.0);
  Query q;
  q.sort = {{1, SortDirection::kAscending}};
  q.filters = {{.column = 1, .op = FilterOp::kLess, .a = 500}};
  q.search = "station";
  view.SetQuery(q);
  view.Update(0.0);
  const std::size_t before = view.rows().size();
  ASSERT_GT(before, 0U);
  t->AddRow({"Station new low", std::int64_t{3}});
  t->AddRow({"Station new high", std::int64_t{900}});
  t->AddRow({"Outpost new", std::int64_t{4}});
  view.Update(1.0);
  EXPECT_EQ(view.view().path, RowView::Path::kAppended);
  // Only the matching new row joined, at its sorted place.
  ASSERT_EQ(view.rows().size(), before + 1);
  for (std::size_t i = 1; i < view.rows().size(); ++i) {
    ASSERT_LE(t->GetInt64(1, view.rows()[i - 1]),
              t->GetInt64(1, view.rows()[i]));
  }
  EXPECT_NE(std::find(view.rows().begin(), view.rows().end(), 1000U),
            view.rows().end());
  // The result equals a full run.
  auto corpus = SearchCorpus::Build(*t, {});
  const auto full = Execute(*t, q, nullptr, corpus.get());
  EXPECT_EQ(std::vector<RowIndex>(view.rows().begin(), view.rows().end()),
            full->rows);
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
