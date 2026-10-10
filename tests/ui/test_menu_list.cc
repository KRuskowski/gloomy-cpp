#include "gloomy/ui/menu_list.h"

#include <gtest/gtest.h>

#include <vector>

#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

auto Items() -> std::vector<MenuListItem> {
  return {{"Continue", "", false, "No saved game yet"},
          {"New game", "Size, seed, difficulty", true, ""},
          {"Load game", "", false, "No saves"},
          {"Settings", "Keys, scale", true, ""},
          {"Quit", "To the desktop", true, ""}};
}

TEST(MenuListTest, StepWrapsAndSkipsDisabledItems) {
  const std::vector<MenuListItem> items = Items();
  EXPECT_EQ(MenuListStep(items, 1, 1), 3);
  EXPECT_EQ(MenuListStep(items, 3, 1), 4);
  EXPECT_EQ(MenuListStep(items, 4, 1), 1);
  EXPECT_EQ(MenuListStep(items, 1, -1), 4);
  const std::vector<MenuListItem> one = {{"Only", "", true, ""},
                                         {"Off", "", false, "x"}};
  EXPECT_EQ(MenuListStep(one, 0, 1), 0);
  EXPECT_EQ(MenuListStep({}, 0, 1), -1);
}

TEST(MenuListTest, KeysMoveTheCursorAndEnterActivates) {
  UiHarness h;
  const std::vector<MenuListItem> items = Items();
  int cursor = 0;
  int activated = -1;
  auto ui = [&](Context &ctx) {
    activated = MenuList(ctx, "##menu", items, cursor);
  };
  h.Frame(ui);
  // The cursor leaves the disabled first item.
  EXPECT_EQ(cursor, 1);
  EXPECT_EQ(activated, -1);
  h.Frame(ui, UiHarness::Key(Key::kDown));
  EXPECT_EQ(cursor, 3);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(activated, 3);
  h.Frame(ui);
  EXPECT_EQ(activated, -1);
}

TEST(MenuListTest, PointerHoversAndClicksOnlyEnabledItems) {
  UiHarness h({800, 600});
  const std::vector<MenuListItem> items = Items();
  int cursor = 1;
  int activated = -1;
  Id quit = kNoId;
  Id load = kNoId;
  auto ui = [&](Context &ctx) {
    activated = MenuList(ctx, "##menu", items, cursor);
    ctx.PushId("##menu");
    quit = ctx.GetId(std::uint64_t{4});
    load = ctx.GetId(std::uint64_t{2});
    ctx.PopId();
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(load)));
  EXPECT_EQ(activated, -1);
  EXPECT_EQ(cursor, 1);
  h.Frame(ui, UiHarness::Click(h.Center(quit)));
  EXPECT_EQ(activated, 4);
  EXPECT_EQ(cursor, 4);
}

TEST(MenuListTest, KeysCanBeLeftToOthers) {
  UiHarness h;
  const std::vector<MenuListItem> items = Items();
  int cursor = 1;
  bool left = false;
  h.Frame(
      [&](Context &ctx) {
        MenuList(ctx, "##menu", items, cursor, {.keys = false});
        left = ctx.ConsumeKey(Key::kDown);
      },
      UiHarness::Key(Key::kDown));
  EXPECT_TRUE(left);
  EXPECT_EQ(cursor, 1);
}

}  // namespace
}  // namespace gloomy::ui
