#include "gloomy/app/app_shell.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "gloomy/core/image.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::app {
namespace {

TEST(AppShellTest, ParsesKeyNames) {
  EXPECT_EQ(ParseKeyName("Down").key, ui::Key::kDown);
  EXPECT_EQ(ParseKeyName("Period").key, ui::Key::kPeriod);
  EXPECT_EQ(ParseKeyName("T").key, ui::Key::kT);
  EXPECT_EQ(ParseKeyName("7").key, ui::Key::k7);
  EXPECT_EQ(ParseKeyName("F9").key, ui::Key::kF9);
  EXPECT_EQ(ParseKeyName("F12").key, ui::Key::kF12);
  const ui::InputEvent ctrl_k = ParseKeyName("Ctrl+K");
  EXPECT_EQ(ctrl_k.key, ui::Key::kK);
  EXPECT_EQ(ctrl_k.mods, ui::kModCtrl);
  EXPECT_EQ(ParseKeyName("Shift+Tab").mods, ui::kModShift);
  EXPECT_EQ(ParseKeyName("Banana").key, ui::Key::kNone);
  EXPECT_EQ(ParseKeyName("F13").key, ui::Key::kNone);
}

TEST(AppShellTest, ParsesKeyLists) {
  std::vector<ui::InputEvent> keys;
  EXPECT_TRUE(ParseKeyList("Down,Down,Enter", keys));
  ASSERT_EQ(keys.size(), 3U);
  EXPECT_EQ(keys[2].key, ui::Key::kEnter);
  keys.clear();
  EXPECT_FALSE(ParseKeyList("Down,Nope", keys));
}

TEST(AppShellTest, ParsesShellArguments) {
  ShellOptions options;
  std::string a0 = "app";
  std::string a1 = "--screenshot";
  std::string a2 = "out.png";
  std::string a3 = "--size";
  std::string a4 = "640x480";
  std::string a5 = "--frames";
  std::string a6 = "9";
  char *argv[] = {a0.data(),
                  a1.data(),
                  a2.data(),
                  a3.data(),
                  a4.data(),
                  a5.data(),
                  a6.data()};
  ParseShellArgs(7, argv, options);
  EXPECT_EQ(options.screenshot, "out.png");
  EXPECT_EQ(options.size.width, 640U);
  EXPECT_EQ(options.size.height, 480U);
  EXPECT_EQ(options.screenshot_frames, 9);
}

// Runs a small app headless: scripted keys reach the UI frame by frame,
// time advances by 1/60 s, and the screenshot is written.
TEST(AppShellTest, HeadlessRunFeedsScriptedInput) {
  const std::filesystem::path png =
      std::filesystem::temp_directory_path() / "gloomy_app_shell_test.png";
  std::vector<std::int64_t> frames_with_key;
  double last_dt = -1.0;
  ShellOptions options;
  options.size = {200, 100};
  options.screenshot = png.string();
  options.screenshot_frames = 5;
  options.headless_input = [](std::int64_t frame,
                              std::vector<ui::InputEvent> &events) {
    if (frame == 2) {
      events.push_back(ui::InputEvent::KeyDown(ui::Key::kPeriod));
    }
  };
  options.frame = [&](ui::Context &ctx, const FrameInfo &info) {
    EXPECT_TRUE(info.headless);
    last_dt = info.dt;
    if (ctx.ConsumeKey(ui::Key::kPeriod)) {
      frames_with_key.push_back(info.frame);
    }
    ui::Label(ctx, "Hello");
  };
  EXPECT_EQ(RunApp(options), 0);
  EXPECT_EQ(frames_with_key, (std::vector<std::int64_t>{2}));
  EXPECT_NEAR(last_dt, 1.0 / 60.0, 1e-9);
  auto image = LoadPng(png.string());
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width, 200);
  std::filesystem::remove(png);
}

}  // namespace
}  // namespace gloomy::app
