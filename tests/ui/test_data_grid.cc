#include "gloomy/ui/data_grid.h"

#include <gtest/gtest.h>

#include <memory>

#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::MarketTable;
using testing::UiHarness;

class DataGridTest : public ::testing::Test {
 protected:
  explicit DataGridTest(int rows = 1000)
      : table_(std::make_shared<MarketTable>(rows)), view_(table_) {}

  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          result_ = DataGrid(ctx, "##market", view_, state_);
        },
        std::move(events));
  }

  auto Body() -> Rect {
    return h_.ctx().LastRect(h_.ctx().GetId("##market")).value();
  }

  // Centre of the header cell of display column `col`.
  auto HeaderPoint(int col) -> Vec2 {
    float x = Body().x0;
    for (int i = 0; i < col; ++i) {
      x += state_.columns[static_cast<std::size_t>(i)].width;
    }
    return {x + state_.columns[static_cast<std::size_t>(col)].width / 2,
            Body().y0 + 10};
  }

  // A point inside display row `pos` (when not scrolled).
  auto RowPoint(int pos) -> Vec2 {
    const float row_h = h_.ctx().theme().metrics.row_height;
    return {Body().x0 + 40, Body().y0 + row_h + 4 + row_h * pos + row_h / 2};
  }

  UiHarness h_{{900, 500}};
  std::shared_ptr<MarketTable> table_;
  data::TableView view_;
  GridState state_;
  GridResult result_;
};

TEST_F(DataGridTest, DefaultColumnsAndVirtualRows) {
  Frame();
  Frame();
  EXPECT_EQ(state_.columns.size(), 7U);
  EXPECT_EQ(state_.columns[3].renderer, CellRenderer::kBadge);
  EXPECT_LT(h_.ctx().node_count(), 10);
  EXPECT_EQ(view_.rows().size(), 1000U);
}

TEST_F(DataGridTest, HeaderClickCyclesSort) {
  Frame();
  Frame(UiHarness::Click(HeaderPoint(4)));
  ASSERT_EQ(state_.query.sort.size(), 1U);
  EXPECT_EQ(state_.query.sort[0].column, 4);
  EXPECT_TRUE(result_.query_changed);
  Frame();
  const auto rows = view_.rows();
  for (std::size_t i = 1; i < rows.size(); ++i) {
    ASSERT_LE(table_->GetDouble(4, rows[i - 1]), table_->GetDouble(4, rows[i]));
  }
  Frame(UiHarness::Click(HeaderPoint(4)));
  EXPECT_EQ(state_.query.sort[0].direction, data::SortDirection::kDescending);
  Frame(UiHarness::Click(HeaderPoint(4)));
  EXPECT_TRUE(state_.query.sort.empty());
}

TEST_F(DataGridTest, ShiftClickAddsSortKeys) {
  Frame();
  Frame(UiHarness::Click(HeaderPoint(1)));
  const Vec2 p = HeaderPoint(5);
  Frame({InputEvent::Move(p.x, p.y),
         InputEvent::Down(p.x, p.y, PointerButton::kLeft, kModShift),
         InputEvent::Up(p.x, p.y)});
  ASSERT_EQ(state_.query.sort.size(), 2U);
  EXPECT_EQ(state_.query.sort[0].column, 1);
  EXPECT_EQ(state_.query.sort[1].column, 5);
}

TEST_F(DataGridTest, DragEdgeResizesColumn) {
  Frame();
  const float edge = Body().x0 + state_.columns[0].width;
  const float y = Body().y0 + 10;
  const float before = state_.columns[0].width;
  Frame({InputEvent::Move(edge, y), InputEvent::Down(edge, y)});
  Frame({InputEvent::Move(edge + 50, y)});
  EXPECT_FLOAT_EQ(state_.columns[0].width, before + 50);
  Frame({InputEvent::Up(edge + 50, y)});
  EXPECT_TRUE(state_.query.sort.empty()) << "a resize is not a sort click";
}

TEST_F(DataGridTest, DragHeaderReordersColumns) {
  Frame();
  const Vec2 a = HeaderPoint(0);
  const Vec2 b = HeaderPoint(1);
  Frame({InputEvent::Move(a.x, a.y), InputEvent::Down(a.x, a.y)});
  Frame({InputEvent::Move(b.x + 20, a.y)});
  Frame({InputEvent::Up(b.x + 20, a.y)});
  EXPECT_EQ(state_.columns[0].column, 1);
  EXPECT_EQ(state_.columns[1].column, 0);
  EXPECT_TRUE(state_.query.sort.empty());
}

TEST_F(DataGridTest, KeyboardSelection) {
  Frame();
  Frame(UiHarness::Key(Key::kTab));
  ASSERT_TRUE(h_.ctx().HasFocus(h_.ctx().GetId("##market")));
  Frame(UiHarness::Key(Key::kDown));
  EXPECT_EQ(state_.cursor_pos, 0);
  EXPECT_TRUE(state_.selection.Contains(0));
  Frame(UiHarness::Key(Key::kDown, kModShift));
  Frame(UiHarness::Key(Key::kDown, kModShift));
  EXPECT_EQ(state_.selection.keys.size(), 3U);
  Frame(UiHarness::Key(Key::kC, kModCtrl));
  const std::string tsv = h_.ctx().clipboard().get();
  EXPECT_TRUE(tsv.starts_with("Station\tSector\tGood\tKind\tPrice"));
  EXPECT_EQ(std::count(tsv.begin(), tsv.end(), '\n'), 4);
  Frame(UiHarness::Key(Key::kEnd));
  EXPECT_EQ(state_.cursor_pos, 999);
  EXPECT_GT(state_.scroll_y, 0.0);
  Frame(UiHarness::Key(Key::kA, kModCtrl));
  EXPECT_TRUE(state_.selection.all);
  Frame(UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(state_.selection.Empty());
  EXPECT_TRUE(h_.ctx().HasFocus(h_.ctx().GetId("##market")))
      << "Escape cleared the selection, not the focus";
  Frame(UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result_.activated);
  EXPECT_EQ(result_.row, 999U);
}

TEST_F(DataGridTest, MouseSelection) {
  Frame();
  Frame(UiHarness::Click(RowPoint(2)));
  EXPECT_EQ(state_.cursor_pos, 2);
  const Vec2 p = RowPoint(5);
  Frame({InputEvent::Move(p.x, p.y),
         InputEvent::Down(p.x, p.y, PointerButton::kLeft, kModShift),
         InputEvent::Up(p.x, p.y)});
  EXPECT_EQ(state_.selection.keys.size(), 4U);
  const Vec2 q = RowPoint(9);
  Frame({InputEvent::Move(q.x, q.y),
         InputEvent::Down(q.x, q.y, PointerButton::kLeft, kModCtrl),
         InputEvent::Up(q.x, q.y)});
  EXPECT_EQ(state_.selection.keys.size(), 5U);
  EXPECT_TRUE(state_.selection.Contains(9));
}

TEST_F(DataGridTest, CursorSurvivesResort) {
  Frame();
  Frame(UiHarness::Click(RowPoint(3)));
  const std::uint64_t key = *state_.cursor;
  state_.query.sort = {{4, data::SortDirection::kDescending}};
  Frame();
  Frame();
  EXPECT_EQ(*state_.cursor, key);
  EXPECT_EQ(view_.rows()[static_cast<std::size_t>(state_.cursor_pos)],
            static_cast<data::RowIndex>(key));
}

TEST_F(DataGridTest, TypeToJumpOnSortedTextColumn) {
  state_.query.sort = {{0, data::SortDirection::kAscending}};
  Frame();
  Frame(UiHarness::Key(Key::kTab));
  Frame({InputEvent::Text("s"),
         InputEvent::Text("t"),
         InputEvent::Text("a"),
         InputEvent::Text("t"),
         InputEvent::Text("i"),
         InputEvent::Text("o"),
         InputEvent::Text("n"),
         InputEvent::Text(" "),
         InputEvent::Text("5")});
  ASSERT_GE(state_.cursor_pos, 0);
  EXPECT_EQ(table_->GetText(
                0, view_.rows()[static_cast<std::size_t>(state_.cursor_pos)]),
            "Station 5");
}

TEST_F(DataGridTest, HeaderMenuHidesColumnAndFilters) {
  Frame();
  const Vec2 p = HeaderPoint(5);
  Frame({InputEvent::Move(p.x, p.y),
         InputEvent::Down(p.x, p.y, PointerButton::kRight),
         InputEvent::Up(p.x, p.y, PointerButton::kRight)});
  Frame();
  ASSERT_EQ(h_.ctx().draw_lists().size(), 2U) << "menu open";
  // Type a filter value and press Enter.
  Id field = kNoId;
  h_.Frame([&](Context &ctx) {
    result_ = DataGrid(ctx, "##market", view_, state_);
    ctx.PushId(HashCombine(ctx.GetId("##market"), 0x6D656E75ULL));
    field = ctx.GetId("##value");
    ctx.PopId();
  });
  h_.ctx().SetFocus(field);
  Frame({InputEvent::Text("50000")});
  Frame(UiHarness::Key(Key::kEnter));
  ASSERT_EQ(state_.query.filters.size(), 1U);
  EXPECT_EQ(state_.query.filters[0].op, data::FilterOp::kGreaterEqual);
  EXPECT_DOUBLE_EQ(state_.query.filters[0].a, 50000.0);
  Frame();
  for (const data::RowIndex r : view_.rows()) {
    ASSERT_GE(table_->GetInt64(5, r), 50000);
  }
  EXPECT_LT(view_.rows().size(), 1000U);
}

TEST_F(DataGridTest, ExplanationTooltipAfterHover) {
  Frame();
  // Hover a price cell.
  const Vec2 p{HeaderPoint(4).x, RowPoint(1).y};
  Frame({InputEvent::Move(p.x, p.y)});
  Frame();
  EXPECT_EQ(h_.ctx().draw_lists().size(), 1U);
  h_.AdvanceTime(0.6);
  Frame();
  EXPECT_EQ(h_.ctx().draw_lists().size(), 2U);
}

TEST_F(DataGridTest, QuickSearchFiltersThroughTheView) {
  state_.query.search = "\"station 12\"";
  Frame();
  Frame();
  EXPECT_EQ(view_.rows().size(), 11U);
  EXPECT_EQ(view_.terms().size(), 1U);
}

TEST_F(DataGridTest, GroupsCollapseAndExpand) {
  state_.query.group_by = {1};
  state_.query.aggregates = {{5, data::AggregateOp::kSum}};
  Frame();
  Frame();
  // 5 sectors, 200 rows each: 5 headers + 1000 rows.
  EXPECT_EQ(state_.display.size(), 1005U);
  EXPECT_EQ(view_.view().groups.size(), 5U);
  // Click the first group header: it collapses.
  Frame(UiHarness::Click(RowPoint(0)));
  Frame();
  EXPECT_EQ(state_.display.size(), 805U);
  EXPECT_EQ(state_.collapsed.size(), 1U);
  // Keyboard: the cursor is on the header; Right expands, Left collapses.
  Frame(UiHarness::Key(Key::kRight));
  Frame();
  EXPECT_EQ(state_.display.size(), 1005U);
  Frame(UiHarness::Key(Key::kLeft));
  Frame();
  EXPECT_EQ(state_.display.size(), 805U);
  // Down moves to the next group header (the first is collapsed).
  Frame(UiHarness::Key(Key::kDown));
  EXPECT_EQ(state_.cursor_pos, 1);
  EXPECT_TRUE(*state_.cursor & kGroupKeyBit);
  // The second group is open: Down lands on its first data row.
  Frame(UiHarness::Key(Key::kDown));
  EXPECT_FALSE(*state_.cursor & kGroupKeyBit);
  Frame(UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result_.activated);
  // Collapsed groups stay collapsed when the query changes.
  state_.query.sort = {{4, data::SortDirection::kAscending}};
  Frame();
  Frame();
  EXPECT_EQ(state_.display.size(), 805U);
}

TEST_F(DataGridTest, GroupByFromTheHeaderMenu) {
  Frame();
  const Vec2 p = HeaderPoint(2);
  Frame({InputEvent::Move(p.x, p.y),
         InputEvent::Down(p.x, p.y, PointerButton::kRight),
         InputEvent::Up(p.x, p.y, PointerButton::kRight)});
  Frame();
  Id item = kNoId;
  h_.Frame([&](Context &ctx) {
    result_ = DataGrid(ctx, "##market", view_, state_);
    ctx.PushId(HashCombine(ctx.GetId("##market"), 0x6D656E75ULL));
    item = ctx.GetId("Group by this column");
    ctx.PopId();
  });
  Frame(UiHarness::Click(h_.Center(item)));
  EXPECT_EQ(state_.query.group_by, (std::vector<int>{2}));
}

class DataGridMillionTest : public DataGridTest {
 protected:
  DataGridMillionTest() : DataGridTest(1000000) {}
};

TEST_F(DataGridMillionTest, MillionRowsStayVirtual) {
  view_.Finish();
  Frame();
  Frame(UiHarness::Key(Key::kTab));
  Frame(UiHarness::Key(Key::kEnd));
  EXPECT_EQ(state_.cursor_pos, 999999);
  // 24 million pixels of content: double precision keeps the last row
  // exactly in view.
  EXPECT_NEAR(
      state_.scroll_y + Body().Height() - 2 * 22.0 - 4, 1000000.0 * 22.0, 30.0);
  EXPECT_LT(h_.ctx().node_count(), 10);
}

}  // namespace
}  // namespace gloomy::ui
