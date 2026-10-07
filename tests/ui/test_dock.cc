#include "gloomy/ui/dock.h"

#include <gtest/gtest.h>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(DockLayoutTest, DockSidesAndClose) {
  DockLayout layout({"markets", "ships"});
  layout.Dock("contract", "markets", DockSide::kRight, 0.4F);
  EXPECT_EQ(layout.Encode(), "s:h:0.600(t:0:markets,ships)(t:0:contract)");
  layout.Dock("log", "contract", DockSide::kBottom);
  EXPECT_EQ(layout.Stacks().size(), 3U);
  layout.Dock("map", "markets", DockSide::kCenter);
  EXPECT_EQ(layout.StackOf("map"), layout.StackOf("markets"));
  // Moving the only panel of a stack removes the stack.
  layout.Dock("log", "markets", DockSide::kCenter);
  EXPECT_EQ(layout.Stacks().size(), 2U);
  layout.Close("contract");
  EXPECT_EQ(layout.Stacks().size(), 1U);
  EXPECT_EQ(layout.Encode(), "t:3:markets,ships,map,log");
  layout.Close("markets");
  layout.Close("ships");
  layout.Close("map");
  layout.Close("log");
  EXPECT_EQ(layout.Encode(), "t:0:");
  layout.Dock("again", "nothing", DockSide::kLeft);
  EXPECT_EQ(layout.Encode(), "t:0:again");
}

TEST(DockLayoutTest, EncodeDecodeRoundTrip) {
  DockLayout layout({"a,b", "c"});
  layout.Dock("d", "c", DockSide::kTop, 0.3F);
  layout.Dock("e", "a,b", DockSide::kLeft);
  const std::string text = layout.Encode();
  const auto back = DockLayout::Decode(text);
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(back->Encode(), text);
  EXPECT_GE(back->StackOf("a,b"), 0);
  EXPECT_FALSE(DockLayout::Decode("s:h:0.5(t:0:a)").has_value());
  EXPECT_FALSE(DockLayout::Decode("nonsense").has_value());
}

class DockSpaceTest : public ::testing::Test {
 protected:
  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          DockSpace(ctx,
                    "##dock",
                    layout_,
                    state_,
                    [](Context &c, const std::string &panel) {
                      Label(c, "Panel " + panel);
                    });
        },
        std::move(events));
  }

  // Centre of the tab of `panel` in the last frame.
  auto Tab(const std::string &panel) -> Vec2 {
    return h_.Center(DockTabId(h_.ctx(), "##dock", panel));
  }

  UiHarness h_{{800, 500}};
  DockLayout layout_{{"markets", "ships", "contract"}};
  DockState state_;
};

TEST_F(DockSpaceTest, ClickActivatesAndCloseRemoves) {
  Frame();
  Frame(UiHarness::Click(Tab("ships")));
  EXPECT_EQ(layout_.nodes()[0].active, 1);
  Frame(UiHarness::Click(h_.Center(DockCloseId(h_.ctx(), "##dock", "ships"))));
  // The right neighbour of the closed active tab becomes active.
  EXPECT_EQ(layout_.Encode(), "t:1:markets,contract");
}

TEST_F(DockSpaceTest, DragTabToTheRightEdgeSplits) {
  Frame();
  const Vec2 from = Tab("contract");
  Frame({InputEvent::Move(from.x, from.y), InputEvent::Down(from.x, from.y)});
  Frame({InputEvent::Move(from.x + 20, from.y + 60)});
  Frame({InputEvent::Move(780, 300)});
  EXPECT_TRUE(state_.drag_active);
  Frame({InputEvent::Up(780, 300)});
  Frame();
  EXPECT_EQ(layout_.Stacks().size(), 2U);
  EXPECT_TRUE(layout_.Encode().starts_with("s:h:"));
  EXPECT_NE(layout_.StackOf("contract"), layout_.StackOf("markets"));
}

TEST_F(DockSpaceTest, KeyboardCyclesStacksAndTabs) {
  layout_.Dock("log", "markets", DockSide::kBottom);
  Frame();
  Frame(UiHarness::Key(Key::kF6));
  const int first = state_.focused_stack;
  Frame(UiHarness::Key(Key::kF6));
  EXPECT_NE(state_.focused_stack, first);
  Frame(UiHarness::Key(Key::kF6));
  EXPECT_EQ(state_.focused_stack, first);
  Frame(UiHarness::Key(Key::kTab, kModCtrl));
  EXPECT_EQ(layout_.nodes()[static_cast<std::size_t>(first)].active, 1);
}

class DockImageTest : public testing::GpuTest {};

TEST_F(DockImageTest, WorkspaceWithDropPreview) {
  UiHarness h({720, 420});
  DockLayout layout({"Markets", "Ships"});
  layout.Dock("Contract", "Markets", DockSide::kRight, 0.42F);
  layout.Dock("Log", "Contract", DockSide::kBottom, 0.6F);
  DockState state;
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(6);
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    DockSpace(
        ctx, "##dock", layout, state, [](Context &c, const std::string &panel) {
          Heading(c, panel);
          LabelOptions muted;
          muted.muted = true;
          muted.wrap = true;
          Label(c, "Drag a tab onto another panel to dock it.", muted);
        });
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  // Start dragging "Ships" and hover the left edge of the Contract stack.
  const Vec2 from = h.Center(DockTabId(h.ctx(), "##dock", "Ships"));
  h.Frame(ui,
          {InputEvent::Move(from.x, from.y), InputEvent::Down(from.x, from.y)});
  h.Frame(ui, {InputEvent::Move(from.x + 30, from.y + 40)});
  h.Frame(ui, {InputEvent::Move(440, 120)});
  h.Frame(ui);
  EXPECT_TRUE(state.drag_active) << state.dragging;
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {720, 420});
  testing::ExpectMatchesReference(
      gfx::RenderToImage(ctx(),
                         renderer,
                         target,
                         h.ctx().draw_lists(),
                         h.ctx().theme().colors.background),
      "dock");
}

}  // namespace
}  // namespace gloomy::ui
