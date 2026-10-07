#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "gloomy/core/parallel.h"
#include "gloomy/data/columnar_table.h"
#include "gloomy/data/query.h"

namespace gloomy::data {
namespace {

auto Columns() -> std::vector<ColumnInfo> {
  ColumnInfo name{.name = "name", .title = "Name", .type = ColumnType::kText};
  ColumnInfo price{
      .name = "price", .title = "Price", .type = ColumnType::kDouble};
  ColumnInfo stock{
      .name = "stock", .title = "Stock", .type = ColumnType::kInt64};
  ColumnInfo kind{.name = "kind", .title = "Kind", .type = ColumnType::kEnum};
  kind.enum_labels = {"Mine", "Smelter", "Farm"};
  return {name, price, stock, kind};
}

// name, price, stock, kind
auto Small() -> ColumnarTable {
  ColumnarTable t(Columns());
  t.AddRow({"beta", 20.0, std::int64_t{5}, std::int64_t{0}});
  t.AddRow({"Alpha", 20.0, std::int64_t{1}, std::int64_t{1}});
  t.AddRow({"gamma", 5.0, std::int64_t{9}, std::int64_t{2}});
  t.AddRow({"delta",
            std::numeric_limits<double>::quiet_NaN(),
            std::int64_t{2},
            std::int64_t{0}});
  t.AddRow({"Alpha", 1.0, std::int64_t{7}, std::int64_t{1}});
  return t;
}

auto RunQuery(const TableSource &t, const Query &q,
              const RowView *prev = nullptr) -> RowView {
  std::shared_ptr<SearchCorpus> corpus = SearchCorpus::Build(t, {});
  auto r = Execute(t, q, prev, corpus.get());
  EXPECT_TRUE(r.has_value());
  return *r;
}

TEST(QueryTest, NoQueryKeepsSourceOrder) {
  const ColumnarTable t = Small();
  EXPECT_EQ(RunQuery(t, {}).rows, (std::vector<RowIndex>{0, 1, 2, 3, 4}));
}

TEST(QueryTest, TextSortIsCaseInsensitiveAndStable) {
  const ColumnarTable t = Small();
  Query q;
  q.sort = {{0, SortDirection::kAscending}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{1, 4, 0, 3, 2}));
}

TEST(QueryTest, MultiColumnSort) {
  const ColumnarTable t = Small();
  Query q;
  q.sort = {{1, SortDirection::kDescending}, {2, SortDirection::kAscending}};
  // Price desc (blank last), then stock asc.
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{1, 0, 2, 4, 3}));
}

TEST(QueryTest, BlanksSortLastBothWays) {
  const ColumnarTable t = Small();
  Query q;
  q.sort = {{1, SortDirection::kAscending}};
  EXPECT_EQ(RunQuery(t, q).rows.back(), 3U);
}

TEST(QueryTest, TypedFilters) {
  const ColumnarTable t = Small();
  Query q;
  q.filters = {{.column = 1, .op = FilterOp::kGreaterEqual, .a = 5.0}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{0, 1, 2}));
  q.filters = {{.column = 2, .op = FilterOp::kBetween, .a = 2, .b = 7}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{0, 3, 4}));
  q.filters = {{.column = 0, .op = FilterOp::kStartsWith, .text = "AL"}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{1, 4}));
  q.filters = {{.column = 3, .op = FilterOp::kInSet, .set = {0, 2}}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{0, 2, 3}));
  q.filters = {{.column = 1, .op = FilterOp::kBlank}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{3}));
  q.filters = {{.column = 0, .op = FilterOp::kNotContains, .text = "a"}};
  EXPECT_TRUE(RunQuery(t, q).rows.empty());
}

TEST(QueryTest, SearchAndFiltersCombine) {
  const ColumnarTable t = Small();
  Query q;
  q.search = "alpha";
  q.filters = {{.column = 2, .op = FilterOp::kGreater, .a = 3}};
  EXPECT_EQ(RunQuery(t, q).rows, (std::vector<RowIndex>{4}));
}

TEST(QueryTest, NarrowingRunsOnPreviousRows) {
  const ColumnarTable t = Small();
  Query a;
  a.search = "a";
  a.sort = {{2, SortDirection::kAscending}};
  const RowView first = RunQuery(t, a);
  EXPECT_EQ(first.path, RowView::Path::kFull);
  Query b = a;
  b.search = "al";
  const RowView second = RunQuery(t, b, &first);
  EXPECT_EQ(second.path, RowView::Path::kNarrowed);
  EXPECT_EQ(second.rows, (std::vector<RowIndex>{1, 4}));
  // Changing only the sort re-sorts the same rows.
  Query c = b;
  c.sort = {{2, SortDirection::kDescending}};
  const RowView third = RunQuery(t, c, &second);
  EXPECT_EQ(third.path, RowView::Path::kResorted);
  EXPECT_EQ(third.rows, (std::vector<RowIndex>{4, 1}));
  // Widening (backspace) needs a full run.
  const RowView fourth = RunQuery(t, a, &second);
  EXPECT_EQ(fourth.path, RowView::Path::kFull);
}

TEST(QueryTest, IsNarrowingRules) {
  Query a;
  a.search = "ore";
  Query b = a;
  b.search = "ore mine";
  EXPECT_TRUE(IsNarrowing(a, b));
  b.filters.push_back({.column = 1, .op = FilterOp::kLess, .a = 3});
  EXPECT_TRUE(IsNarrowing(a, b));
  EXPECT_FALSE(IsNarrowing(b, a));
  b.sort = {{0, SortDirection::kAscending}};
  EXPECT_FALSE(IsNarrowing(a, b));
}

TEST(QueryTest, ParallelSortEqualsSequential) {
  ColumnarTable t(Columns());
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 999);
  for (int i = 0; i < 300000; ++i) {
    const std::string name = "n" + std::to_string(dist(rng));
    t.AddRow({std::string_view(name),
              static_cast<double>(dist(rng)),
              std::int64_t{dist(rng)},
              std::int64_t{dist(rng) % 3}});
  }
  Query q;
  q.sort = {{0, SortDirection::kAscending}, {1, SortDirection::kDescending}};
  SetWorkerThreads(4);
  const RowView parallel = RunQuery(t, q);
  SetWorkerThreads(1);
  const RowView sequential = RunQuery(t, q);
  SetWorkerThreads(4);
  EXPECT_EQ(parallel.rows, sequential.rows);
  ASSERT_EQ(parallel.rows.size(), 300000U);
  for (std::size_t i = 1; i < parallel.rows.size(); ++i) {
    const auto a = t.GetText(0, parallel.rows[i - 1]);
    const auto b = t.GetText(0, parallel.rows[i]);
    ASSERT_LE(a, b);
  }
}

TEST(QueryTest, CancelStopsExecution) {
  const ColumnarTable t = Small();
  std::atomic<bool> cancel{true};
  Query q;
  q.filters = {{.column = 2, .op = FilterOp::kGreater, .a = 0}};
  EXPECT_FALSE(Execute(t, q, nullptr, nullptr, &cancel).has_value());
}

}  // namespace
}  // namespace gloomy::data
