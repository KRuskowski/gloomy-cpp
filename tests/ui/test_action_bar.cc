#include "gloomy/ui/action_bar.h"

#include <gtest/gtest.h>

#include <vector>

#include "gloomy/ui/overlays.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

using F = ActionFit;

TEST(ActionBarTest, EverythingFullWhenItFits) {
  const std::vector<float> full = {100, 100, 100};
  const std::vector<float> small = {50, 50, 50};
  const std::vector<int> prio = {1, 2, 3};
  EXPECT_EQ(FitActions(full, small, prio, 320, 10, 60),
            (std::vector<F>{F::kFull, F::kFull, F::kFull}));
}

TEST(ActionBarTest, LowestPriorityCompactsFirstThenMovesToTheMenu) {
  const std::vector<float> full = {100, 100, 100};
  const std::vector<float> small = {50, 50, 50};
  const std::vector<int> prio = {1, 3, 2};
  // 320 needed: one compact (the first, lowest) gives 270.
  EXPECT_EQ(FitActions(full, small, prio, 300, 10, 60),
            (std::vector<F>{F::kCompact, F::kFull, F::kFull}));
  // Two compact: 220.
  EXPECT_EQ(FitActions(full, small, prio, 230, 10, 60),
            (std::vector<F>{F::kCompact, F::kFull, F::kCompact}));
  // All compact: 170.
  EXPECT_EQ(FitActions(full, small, prio, 170, 10, 60),
            (std::vector<F>{F::kCompact, F::kCompact, F::kCompact}));
  // Then into the menu, lowest first: 50 + 50 + 60 + 20 = 180 > 160;
  // two in the menu: 50 + 60 + 10 = 120.
  EXPECT_EQ(FitActions(full, small, prio, 160, 10, 60),
            (std::vector<F>{F::kMenu, F::kCompact, F::kMenu}));
  // Nothing fits: all in the menu.
  EXPECT_EQ(FitActions(full, small, prio, 10, 10, 60),
            (std::vector<F>{F::kMenu, F::kMenu, F::kMenu}));
}

TEST(ActionBarTest, TiesYieldFromTheEndAndNoCompactSkipsToTheMenu) {
  const std::vector<float> full = {100, 100};
  // The second has no compact label (as wide as its full one).
  const std::vector<float> small = {60, 100};
  const std::vector<int> prio = {0, 0};
  EXPECT_EQ(FitActions(full, small, prio, 175, 10, 40),
            (std::vector<F>{F::kCompact, F::kFull}))
      << "the later one has no compact label, so the first compacts";
  EXPECT_EQ(FitActions(full, small, prio, 120, 10, 40),
            (std::vector<F>{F::kCompact, F::kMenu}));
}

/// Four actions in a bar of the given width.
struct Bar {
  std::vector<BarAction> actions = {
      {"Company (M)", "M", 1, ButtonKind::kNormal, {}, "Your company"},
      {"Fleet (O): 2 need you", "Fleet: 2", 3, ButtonKind::kNormal, {}, {}},
      {"What should I do? (A)", "Advice (A)", 4, ButtonKind::kPrimary, {}, {}},
      {"Search (Ctrl+K)", "Search", 0, ButtonKind::kNormal, {}, {}},
  };
  int chosen = -1;
  ActionBarResult result;

  void Draw(Context &ctx, float width) {
    LayoutSpec box;
    box.width = SizeSpec::Fixed(width);
    BeginRow(ctx, box);
    result = ActionBar(ctx, "##bar", actions);
    chosen = result.chosen;
    End(ctx);
  }
};

TEST(ActionBarTest, WideBarShowsEveryLabel) {
  UiHarness h({1200, 200});
  Bar bar;
  auto ui = [&](Context &ctx) {
    bar.Draw(ctx, 1000);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##bar"));
  const Id advice = c.GetId("What should I do? (A)##bar2");
  const Id more = c.GetId("More");
  c.PopId();
  EXPECT_TRUE(c.LastRect(advice).has_value());
  EXPECT_FALSE(c.LastRect(more).has_value()) << "no menu needed";
  EXPECT_EQ(bar.result.in_menu, 0);
  EXPECT_EQ(bar.result.compact, 0);
  h.Frame(ui, UiHarness::Click(h.Center(advice)));
  EXPECT_EQ(bar.chosen, 2);
}

TEST(ActionBarTest, NarrowBarNeverCutsAndKeepsTheImportantOnes) {
  UiHarness h({1200, 200});
  Bar bar;
  auto ui = [&](Context &ctx) {
    bar.Draw(ctx, 200);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  const Rect box = c.LastRect(c.GetId("##bar")).value();
  EXPECT_LE(box.Width(), 200.5F);
  c.PushId(c.GetId("##bar"));
  const Id more = c.GetId("More");
  const Id advice_full = c.GetId("What should I do? (A)##bar2");
  const Id advice_compact = c.GetId("Advice (A)##bar2");
  c.PopId();
  ASSERT_TRUE(c.LastRect(more).has_value()) << "the rest in a menu";
  EXPECT_TRUE(c.LastRect(advice_full).has_value() ||
              c.LastRect(advice_compact).has_value())
      << "the highest priority stays in the bar";
  // Every button shown lies inside the bar: nothing cut.
  EXPECT_LE(c.LastRect(more)->x1, box.x1 + 0.5F);
  EXPECT_EQ(bar.result.in_menu, 3);
  // The menu offers the actions that moved there.
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  h.Frame(ui);
  EXPECT_TRUE(bar.result.menu_open);
  int chosen = -1;
  // Down to the first item, Enter chooses it.
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  chosen = bar.chosen;
  EXPECT_GE(chosen, 0);
  EXPECT_NE(chosen, 2) << "the advisor is not in the menu";
  EXPECT_TRUE(bar.result.from_menu);
}

TEST(ActionBarTest, DisabledActionNeverActs) {
  UiHarness h({1200, 200});
  Bar bar;
  bar.actions[0].disabled = "No company";
  auto ui = [&](Context &ctx) {
    bar.Draw(ctx, 1000);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##bar"));
  const Id company = c.GetId("Company (M)##bar0");
  c.PopId();
  ASSERT_TRUE(c.LastRect(company).has_value());
  h.Frame(ui, UiHarness::Click(h.Center(company)));
  EXPECT_EQ(bar.chosen, -1);
}

}  // namespace
}  // namespace gloomy::ui
