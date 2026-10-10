#include "gloomy/ui/list_rows.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

const std::vector<CellColumn> kColumns = {
    {"Ship", 120, CellAlign::kLeft, "The ship's name"},
    {"Order", 0, CellAlign::kLeft, {}},
    {"Cr/h", 70, CellAlign::kRight, {}},
};

const std::vector<RowAction> kSlots = {{"Edit", {}, ButtonKind::kFlat},
                                       {"Show", {}, ButtonKind::kFlat}};

/// A header and three rows; the second has only one of the two actions.
struct List {
  std::vector<CellRowResult> results = std::vector<CellRowResult>(3);
  int selected = -1;

  void Draw(Context &ctx) {
    BeginColumn(ctx);
    CellHeader(ctx, "##head", kColumns, kSlots);
    for (int i = 0; i < 3; ++i) {
      static const char *const kNames[] = {"Courier 1", "Freighter", "Miner"};
      const std::vector<Cell> cells = {
          {kNames[i],
           Tone::kNeutral,
           {},
           true,
           false,
           i == 2 ? "Its name" : ""},
          {"Auto trade with a very long order text that has to be cut "
           "somewhere",
           Tone::kNeutral,
           {},
           false,
           false,
           {}},
          {"-120", Tone::kBad, {}, false, false, {}},
      };
      const std::vector<RowAction> one = {{"Edit", {}, ButtonKind::kFlat}};
      const std::vector<RowAction> two = {
          {"Edit", {}, ButtonKind::kFlat},
          {"Show", i == 0 ? "Lost" : "", ButtonKind::kFlat}};
      CellRowOptions o;
      o.selected = selected == i;
      o.status = i == 0 ? Tone::kBad : Tone::kNeutral;
      o.actions = i == 1 ? std::span<const RowAction>(one)
                         : std::span<const RowAction>(two);
      o.action_slots = kSlots;
      results[static_cast<std::size_t>(i)] =
          CellRow(ctx, "##row" + std::to_string(i), kColumns, cells, o);
    }
    End(ctx);
  }
};

auto CellId(Context &ctx, const std::string &row, int column) -> Id {
  ctx.PushId(ctx.GetId(row));
  const Id id = ctx.GetId("##c" + std::to_string(column));
  ctx.PopId();
  return id;
}

TEST(ListRowsTest, AnEmptyColourInitialiserMeansNone) {
  // Positional initialisers pass {} for the colour; that must not be
  // Color{}, which is opaque black and hid the text on dark panels.
  const Theme theme;
  const Cell plain{"360", Tone::kNeutral, {}, false, false, {}};
  EXPECT_FALSE(plain.color.has_value());
  EXPECT_FALSE(CellTextColor(theme, plain).has_value());
  const Cell bad{"-120", Tone::kBad, {}, false, false, {}};
  ASSERT_TRUE(CellTextColor(theme, bad).has_value());
  EXPECT_EQ(CellTextColor(theme, bad)->r, ToneColor(theme, Tone::kBad).r);
  EXPECT_EQ(CellTextColor(theme, bad)->a, ToneColor(theme, Tone::kBad).a);
  Cell explicit_colour = bad;
  explicit_colour.color = Color{0.2F, 0.4F, 0.6F, 1.0F};
  EXPECT_EQ(CellTextColor(theme, explicit_colour)->g, 0.4F);
}

TEST(ListRowsTest, ColumnsLineUpWithTheHeader) {
  UiHarness h({900, 400});
  List list;
  auto ui = [&](Context &ctx) {
    list.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  for (int col = 0; col < 3; ++col) {
    const Rect head = c.LastRect(CellId(c, "##head", col)).value();
    for (int row = 0; row < 3; ++row) {
      const Rect cell =
          c.LastRect(CellId(c, "##row" + std::to_string(row), col)).value();
      EXPECT_NEAR(cell.x0, head.x0, 0.5F) << "column " << col << " row " << row;
      EXPECT_NEAR(cell.x1, head.x1, 0.5F) << "column " << col << " row " << row;
    }
  }
  const Rect ship = c.LastRect(CellId(c, "##row0", 0)).value();
  EXPECT_NEAR(ship.Width(), 120.0F, 0.5F) << "fixed width at the base size";
  const Rect head = c.LastRect(c.GetId("##head")).value();
  EXPECT_LT(head.Height(), 30.0F) << "the header is one line of titles";
  const Rect order = c.LastRect(CellId(c, "##row0", 1)).value();
  EXPECT_GT(order.Width(), 300.0F) << "the 0 column takes the rest";
  const Rect row = c.LastRect(c.GetId("##row0")).value();
  EXPECT_LE(row.x1, 900.0F) << "a long text is cut, not widening the row";
}

TEST(ListRowsTest, FixedWidthsFollowTheThemeScale) {
  UiHarness h({1200, 400});
  h.ctx().SetTheme(h.ctx().theme().Scaled(1.5F));
  EXPECT_NEAR(CellScale(h.ctx().theme()), 1.5F, 1e-4F);
  List list;
  auto ui = [&](Context &ctx) {
    list.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect ship = h.ctx().LastRect(CellId(h.ctx(), "##row0", 0)).value();
  EXPECT_NEAR(ship.Width(), 180.0F, 0.5F);
}

TEST(ListRowsTest, ClicksSelectAndButtonsAct) {
  UiHarness h({900, 400});
  List list;
  auto ui = [&](Context &ctx) {
    list.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  // A click on the row.
  const Rect row = c.LastRect(c.GetId("##row1")).value();
  h.Frame(ui, UiHarness::Click({row.x0 + 300, row.Center().y}));
  EXPECT_TRUE(list.results[1].clicked);
  EXPECT_EQ(list.results[1].action, -1);
  // A click on a cell with a tooltip is a click on its row too.
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(CellId(c, "##row2", 0))));
  EXPECT_TRUE(list.results[2].clicked);
  // Enter on the focused row activates it.
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(list.results[2].activated);
  // The second button of row 2.
  c.PushId(c.GetId("##row2"));
  const Id show = c.GetId("Show");
  c.PopId();
  h.Frame(ui, UiHarness::Click(h.Center(show)));
  EXPECT_EQ(list.results[2].action, 1);
  EXPECT_FALSE(list.results[2].clicked) << "a button is not the row";
  // Disabled: never acts.
  c.PushId(c.GetId("##row0"));
  const Id lost = c.GetId("Show");
  c.PopId();
  ASSERT_TRUE(c.LastRect(lost).has_value());
  h.Frame(ui, UiHarness::Click(h.Center(lost)));
  EXPECT_EQ(list.results[0].action, -1);
}

TEST(ListRowsTest, FewerActionsKeepTheirSlots) {
  UiHarness h({900, 400});
  List list;
  auto ui = [&](Context &ctx) {
    list.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  const Rect one = c.LastRect(CellId(c, "##row1", 2)).value();
  const Rect two = c.LastRect(CellId(c, "##row0", 2)).value();
  EXPECT_NEAR(one.x1, two.x1, 0.5F);
  EXPECT_GT(RowActionsWidth(c, kSlots, 4), RowActionsWidth(c, {kSlots}, 4) - 1);
  EXPECT_FLOAT_EQ(RowActionsWidth(c, {}, 4), 0.0F);
}

TEST(ListRowsTest, ApplyQueueEditMovesAndRemoves) {
  std::vector<int> q = {1, 2, 3};
  EXPECT_EQ(ApplyQueueEdit(q, 1, QueueEdit::kUp), 0);
  EXPECT_EQ(q, (std::vector<int>{2, 1, 3}));
  EXPECT_EQ(ApplyQueueEdit(q, 0, QueueEdit::kUp), 0) << "already first";
  EXPECT_EQ(ApplyQueueEdit(q, 0, QueueEdit::kDown), 1);
  EXPECT_EQ(q, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(ApplyQueueEdit(q, 2, QueueEdit::kDown), 2) << "already last";
  EXPECT_EQ(ApplyQueueEdit(q, 1, QueueEdit::kGo), 1);
  EXPECT_EQ(ApplyQueueEdit(q, 1, QueueEdit::kRemove), -1);
  EXPECT_EQ(q, (std::vector<int>{1, 3}));
  EXPECT_EQ(ApplyQueueEdit(q, 5, QueueEdit::kUp), -1) << "out of range";
}

/// A queue of three, the middle one running, edited through QueueRow().
struct Queue {
  std::vector<std::string> items = {"Dock at Farm", "Sell", "Buy Ore"};
  int current = 1;
  QueueEdit last = QueueEdit::kNone;

  void Draw(Context &ctx) {
    BeginColumn(ctx);
    last = QueueEdit::kNone;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
      QueueRowSpec spec;
      spec.index = i;
      spec.count = static_cast<int>(items.size());
      spec.text = items[static_cast<std::size_t>(i)];
      spec.current = i == current;
      ctx.PushId(static_cast<std::uint64_t>(i));
      const QueueEdit e = QueueRow(ctx, "##q", spec);
      ctx.PopId();
      if (e != QueueEdit::kNone) {
        last = e;
        ApplyQueueEdit(items, i, e);
        break;
      }
    }
    End(ctx);
  }

  static auto Button(Context &ctx, int row, const char *name) -> Id {
    ctx.PushId(static_cast<std::uint64_t>(row));
    ctx.PushId(ctx.GetId("##q"));
    const Id id = ctx.GetId(name);
    ctx.PopId();
    ctx.PopId();
    return id;
  }
};

TEST(ListRowsTest, QueueRowButtonsEditTheQueue) {
  UiHarness h({900, 300});
  Queue q;
  auto ui = [&](Context &ctx) {
    q.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  // The first cannot move up: disabled, says why, does nothing.
  h.Frame(ui, UiHarness::Click(h.Center(Queue::Button(c, 0, "Up##up"))));
  EXPECT_EQ(q.last, QueueEdit::kNone);
  h.Frame(ui, UiHarness::Click(h.Center(Queue::Button(c, 2, "Up##up"))));
  EXPECT_EQ(q.last, QueueEdit::kUp);
  EXPECT_EQ(q.items[1], "Buy Ore");
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(Queue::Button(c, 1, "Go##go"))));
  EXPECT_EQ(q.last, QueueEdit::kNone) << "the running one has no Go";
  h.Frame(ui, UiHarness::Click(h.Center(Queue::Button(c, 0, "Go##go"))));
  EXPECT_EQ(q.last, QueueEdit::kGo);
  h.Frame(ui,
          UiHarness::Click(h.Center(Queue::Button(c, 0, "Remove##remove"))));
  EXPECT_EQ(q.last, QueueEdit::kRemove);
  EXPECT_EQ(q.items.size(), 2U);
}

TEST(ListRowsTest, QueueRowKeys) {
  UiHarness h({900, 300});
  Queue q;
  q.current = -1;
  auto ui = [&](Context &ctx) {
    q.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(std::uint64_t{2});
  const Id row = c.GetId("##q");
  c.PopId();
  c.SetFocus(row);
  h.Frame(ui, UiHarness::Key(Key::kDown, kModAlt));
  EXPECT_EQ(q.last, QueueEdit::kNone) << "the last cannot move down";
  h.Frame(ui, UiHarness::Key(Key::kUp, kModAlt));
  EXPECT_EQ(q.last, QueueEdit::kUp);
  EXPECT_EQ(q.items[1], "Buy Ore");
  c.SetFocus(row);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(q.last, QueueEdit::kGo);
  h.Frame(ui, UiHarness::Key(Key::kDelete));
  EXPECT_EQ(q.last, QueueEdit::kRemove);
  EXPECT_EQ(q.items.size(), 2U);
}

}  // namespace
}  // namespace gloomy::ui
