#include "gloomy/ui/overlays.h"

#include <gtest/gtest.h>

#include <memory>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/data_grid.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(OverlaysTest, ContextMenuByKeyboardAndMouse) {
  UiHarness h;
  std::string chosen;
  auto ui = [&](Context &ctx) {
    if (BeginContextMenu(ctx, "##menu")) {
      if (MenuItem(ctx, "Dock"))
        chosen = "dock";
      if (MenuItem(ctx, "Disabled", {}, false))
        chosen = "disabled";
      if (MenuItem(ctx, "Trade", "T"))
        chosen = "trade";
      EndContextMenu(ctx);
    }
  };
  h.Frame(ui, {InputEvent::Move(100, 100)});
  OpenContextMenu(h.ctx(), "##menu");
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(chosen, "trade");
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U) << "closed after choosing";
  // Mouse: the first item sits just below the opening point.
  h.Frame(ui, {InputEvent::Move(100, 100)});
  OpenContextMenu(h.ctx(), "##menu");
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click({140, 100 + 4 + 12}));
  EXPECT_EQ(chosen, "dock");
}

TEST(OverlaysTest, DialogClosesOnEscapeAndButton) {
  UiHarness h;
  bool open = true;
  bool pressed = false;
  auto ui = [&](Context &ctx) {
    if (BeginDialog(ctx, "##confirm", "Sell cargo", open)) {
      Label(ctx, "Sell 500 ore for 12,500 Cr?");
      pressed = Button(ctx, "Sell");
      EndDialog(ctx, open);
    }
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_FALSE(open);
  open = true;
  h.Frame(ui);
  Id close = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##confirm"));
    close = ctx.GetId("Close##dialog");
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(close)));
  EXPECT_FALSE(open);
  (void)pressed;
}

TEST(OverlaysTest, ToastsExpire) {
  UiHarness h;
  ToastQueue toasts;
  auto ui = [&](Context &ctx) {
    Toasts(ctx, toasts);
  };
  toasts.Show("Saved view", Tone::kGood, 1.0);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
  h.AdvanceTime(1.5);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
}

TEST(OverlaysTest, SplitDividerDragsAndKeys) {
  UiHarness h({806, 400});
  SplitState split;
  auto ui = [&](Context &ctx) {
    BeginSplit(ctx, "##split", split);
    Label(ctx, "left");
    SplitNext(ctx, split);
    Label(ctx, "right");
    EndSplit(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Id divider = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##split"));
    divider = ctx.GetId("##divider");
    ctx.PopId();
  });
  const Rect d = h.ctx().LastRect(divider).value();
  EXPECT_NEAR(d.x0, 400, 1);
  const Vec2 c = d.Center();
  h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Down(c.x, c.y)});
  h.Frame(ui, {InputEvent::Move(c.x + 200, c.y)});
  h.Frame(ui, {InputEvent::Up(c.x + 200, c.y)});
  EXPECT_NEAR(split.ratio, 0.75F, 0.01F);
  h.Frame(ui, UiHarness::Key(Key::kLeft));
  EXPECT_NEAR(split.ratio, 0.70F, 0.01F);
}

TEST(OverlaysTest, GridCellMenuFiltersByValue) {
  UiHarness h({900, 500});
  auto table = std::make_shared<testing::MarketTable>(500);
  data::TableView view(table);
  GridState state;
  GridResult result;
  auto ui = [&](Context &ctx) {
    result = DataGrid(ctx, "##grid", view, state);
    GridCellMenu(ctx, "##cell_menu", view, state, result);
  };
  h.Frame(ui);
  // Right click the Sector cell of the first row.
  const Rect body = h.ctx().LastRect(h.ctx().GetId("##grid")).value();
  const Vec2 p{body.x0 + state.columns[0].width + 20, body.y0 + 26 + 11};
  h.Frame(ui,
          {InputEvent::Move(p.x, p.y),
           InputEvent::Down(p.x, p.y, PointerButton::kRight),
           InputEvent::Up(p.x, p.y, PointerButton::kRight)});
  h.Frame(ui);
  ASSERT_EQ(h.ctx().draw_lists().size(), 2U);
  // Items: copy cell, copy rows, only, hide, clear.
  for (int i = 0; i < 2; ++i) {
    h.Frame(ui, UiHarness::Key(Key::kDown));
  }
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  ASSERT_EQ(state.query.filters.size(), 1U);
  EXPECT_EQ(state.query.filters[0].op, data::FilterOp::kTextEquals);
  EXPECT_EQ(state.query.filters[0].text, "Argon Prime");
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_EQ(view.rows().size(), 100U);
}

class OverlaysImageTest : public testing::GpuTest {};

TEST_F(OverlaysImageTest, MenuDialogToastSplit) {
  UiHarness h({720, 420});
  SplitState split;
  split.ratio = 0.35F;
  ToastQueue toasts;
  toasts.Show("View \"Cheap ore\" saved", Tone::kGood, 60);
  bool dialog = false;
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(8);
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    BeginSplit(ctx, "##split", split);
    BeginPanel(ctx, {.width = SizeSpec::Grow(), .height = SizeSpec::Grow()});
    Heading(ctx, "Sectors");
    Label(ctx, "Argon Prime");
    Label(ctx, "Ore Belt");
    End(ctx);
    SplitNext(ctx, split);
    BeginPanel(ctx, {.width = SizeSpec::Grow(), .height = SizeSpec::Grow()});
    Heading(ctx, "Details");
    LabelOptions wrap;
    wrap.wrap = true;
    Label(ctx,
          "Right click opens the menu; the divider on the left drags.",
          wrap);
    End(ctx);
    EndSplit(ctx);
    End(ctx);
    if (BeginContextMenu(ctx, "##menu")) {
      MenuItem(ctx, "Plot course", "Enter");
      MenuItem(ctx, "Dock", "D");
      MenuSeparator(ctx);
      MenuItem(ctx, "Trade (not docked)", {}, false);
      EndContextMenu(ctx);
    }
    Toasts(ctx, toasts);
    if (BeginDialog(ctx, "##dlg", "Unused", dialog)) {
      EndDialog(ctx, dialog);
    }
  };
  h.Frame(ui, {InputEvent::Move(300, 120)});
  OpenContextMenu(h.ctx(), "##menu");
  for (int i = 0; i < 3; ++i) {
    h.Frame(ui);
  }
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {720, 420});
  testing::ExpectMatchesReference(
      gfx::RenderToImage(ctx(),
                         renderer,
                         target,
                         h.ctx().draw_lists(),
                         h.ctx().theme().colors.background),
      "overlays");
}

}  // namespace
}  // namespace gloomy::ui
