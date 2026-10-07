#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "gloomy/data/columnar_table.h"
#include "gloomy/data/display_rows.h"
#include "gloomy/data/query.h"
#include "gloomy/data/search.h"

namespace gloomy::data {
namespace {

// sector, good, price
auto Table() -> ColumnarTable {
  ColumnInfo sector{
      .name = "sector", .title = "Sector", .type = ColumnType::kText};
  ColumnInfo good{.name = "good", .title = "Good", .type = ColumnType::kText};
  ColumnInfo price{
      .name = "price", .title = "Price", .type = ColumnType::kDouble};
  ColumnarTable t({sector, good, price});
  t.AddRow({"B", "ore", 10.0});
  t.AddRow({"A", "ore", 30.0});
  t.AddRow({"B", "food", 5.0});
  t.AddRow({"A", "ore", 20.0});
  t.AddRow({"B", "ore", 40.0});
  return t;
}

auto RunQuery(const TableSource &t, const Query &q) -> RowView {
  return *Execute(t, q, nullptr, nullptr);
}

TEST(GroupingTest, GroupsAndAggregates) {
  const ColumnarTable t = Table();
  Query q;
  q.group_by = {0};
  q.sort = {{2, SortDirection::kDescending}};
  q.aggregates = {
      {2, AggregateOp::kSum}, {2, AggregateOp::kMax}, {2, AggregateOp::kCount}};
  const RowView v = RunQuery(t, q);
  // Grouped by sector ascending, then price descending inside.
  EXPECT_EQ(v.rows, (std::vector<RowIndex>{1, 3, 4, 0, 2}));
  ASSERT_EQ(v.groups.size(), 2U);
  EXPECT_EQ(v.groups[0].key, "A");
  EXPECT_EQ(v.groups[0].count, 2U);
  EXPECT_DOUBLE_EQ(v.groups[0].values[0], 50.0);
  EXPECT_DOUBLE_EQ(v.groups[1].values[1], 40.0);
  EXPECT_DOUBLE_EQ(v.groups[1].values[2], 3.0);
}

TEST(GroupingTest, SortKeyOnGroupColumnSetsGroupOrder) {
  Query q;
  q.group_by = {0};
  q.sort = {{0, SortDirection::kDescending}, {2, SortDirection::kAscending}};
  const auto keys = EffectiveSort(q);
  ASSERT_EQ(keys.size(), 2U);
  EXPECT_EQ(keys[0].direction, SortDirection::kDescending);
  EXPECT_EQ(keys[1].column, 2);
}

TEST(GroupingTest, NestedGroupsAndDisplayRows) {
  const ColumnarTable t = Table();
  Query q;
  q.group_by = {0, 1};
  const RowView v = RunQuery(t, q);
  // A, A/ore, B, B/food, B/ore.
  ASSERT_EQ(v.groups.size(), 5U);
  EXPECT_EQ(v.groups[1].parent, 0);
  EXPECT_EQ(v.groups[1].level, 1);
  EXPECT_EQ(v.groups[3].key, "food");
  DisplayRows d;
  d.Build(v, {});
  // 5 headers + 5 rows.
  EXPECT_EQ(d.size(), 10U);
  EXPECT_EQ(d.At(0).group, 0);
  EXPECT_EQ(d.At(1).group, 1);
  EXPECT_EQ(d.At(2).group, -1);
  // Collapse B: its subgroups and rows disappear.
  d.Build(v, {v.groups[2].id});
  EXPECT_EQ(d.size(), 5U);
  EXPECT_EQ(d.LineOfGroup(3), -1);
  EXPECT_EQ(d.LineOfPos(4), -1);
  EXPECT_EQ(d.LineOfPos(0), 2);
  // Ids are stable across queries.
  Query q2 = q;
  q2.filters = {{.column = 2, .op = FilterOp::kGreater, .a = 15}};
  const RowView v2 = RunQuery(t, q2);
  EXPECT_EQ(v2.groups[0].id, v.groups[0].id);
}

TEST(GroupingTest, IdentityWithoutGrouping) {
  const ColumnarTable t = Table();
  const RowView v = RunQuery(t, {});
  DisplayRows d;
  d.Build(v, {});
  EXPECT_FALSE(d.grouped());
  EXPECT_EQ(d.size(), 5U);
  EXPECT_EQ(d.At(3).pos, 3U);
  EXPECT_EQ(d.LineOfPos(2), 2);
}

// name, parent (row key, -1 for none), value
auto Tree() -> ColumnarTable {
  ColumnInfo name{.name = "name", .title = "Name", .type = ColumnType::kText};
  ColumnInfo parent{
      .name = "parent", .title = "Parent", .type = ColumnType::kInt64};
  parent.searchable = false;
  ColumnInfo value{
      .name = "value", .title = "Value", .type = ColumnType::kInt64};
  ColumnarTable t({name, parent, value});
  t.AddRow({"Argon Prime", std::int64_t{-1}, std::int64_t{2}});     // 0
  t.AddRow({"Ore Belt", std::int64_t{-1}, std::int64_t{1}});        // 1
  t.AddRow({"Trading Station", std::int64_t{0}, std::int64_t{5}});  // 2
  t.AddRow({"Shipyard", std::int64_t{0}, std::int64_t{3}});         // 3
  t.AddRow({"Ore Mine", std::int64_t{1}, std::int64_t{7}});         // 4
  t.AddRow({"Freighter", std::int64_t{2}, std::int64_t{9}});        // 5
  return t;
}

TEST(TreeTest, DepthFirstWithSortedSiblings) {
  const ColumnarTable t = Tree();
  Query q;
  q.tree_parent = 1;
  q.sort = {{2, SortDirection::kAscending}};
  const RowView v = RunQuery(t, q);
  // Roots by value: Ore Belt (1), Argon Prime (2); Argon's children by
  // value: Shipyard (3), Trading Station (5) with its Freighter.
  EXPECT_EQ(v.rows, (std::vector<RowIndex>{1, 4, 0, 3, 2, 5}));
  EXPECT_EQ(v.depth, (std::vector<std::uint16_t>{0, 1, 0, 1, 1, 2}));
  EXPECT_TRUE(v.tree_flags[2] & kTreeHasChildren);
  EXPECT_FALSE(v.tree_flags[3] & kTreeHasChildren);
}

TEST(TreeTest, MatchesKeepTheirAncestorsAsContext) {
  const ColumnarTable t = Tree();
  auto corpus = SearchCorpus::Build(t, {});
  Query q;
  q.tree_parent = 1;
  q.search = "freighter";
  const RowView v = *Execute(t, q, nullptr, corpus.get());
  EXPECT_EQ(v.rows, (std::vector<RowIndex>{0, 2, 5}));
  EXPECT_TRUE(v.tree_flags[0] & kTreeContext);
  EXPECT_TRUE(v.tree_flags[1] & kTreeContext);
  EXPECT_FALSE(v.tree_flags[2] & kTreeContext);
}

TEST(TreeTest, CollapsedRowsHideDescendants) {
  const ColumnarTable t = Tree();
  Query q;
  q.tree_parent = 1;
  const RowView v = RunQuery(t, q);
  // Source order: Argon(0) [Trading(2) [Freighter(5)], Shipyard(3)],
  // Ore Belt(1) [Mine(4)].
  ASSERT_EQ(v.rows, (std::vector<RowIndex>{0, 2, 5, 3, 1, 4}));
  DisplayRows d;
  d.Build(v, {t.RowKey(0)}, &t);
  // Argon Prime, Ore Belt, Ore Mine.
  EXPECT_EQ(d.size(), 3U);
  d.Build(v, {t.RowKey(2)}, &t);
  EXPECT_EQ(d.size(), 5U);
  EXPECT_EQ(d.LineOfPos(2), -1);
}

}  // namespace
}  // namespace gloomy::data
