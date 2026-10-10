#include "gloomy/ui/key_chord.h"

#include <gtest/gtest.h>

#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(KeyChordTest, NamesReadBackForEveryKey) {
  for (std::size_t k = 1; k < kKeyCount; ++k) {
    for (const std::uint8_t mods :
         {std::uint8_t{0},
          static_cast<std::uint8_t>(kModCtrl),
          static_cast<std::uint8_t>(kModCtrl | kModShift),
          static_cast<std::uint8_t>(kModAlt)}) {
      const KeyChord chord{static_cast<Key>(k), mods};
      const std::string name = KeyChordName(chord);
      ASSERT_NE(name, "-") << k;
      EXPECT_EQ(ParseKeyChord(name), chord) << name;
    }
  }
  EXPECT_EQ(KeyChordName({Key::kK, kModCtrl}), "Ctrl+K");
  EXPECT_EQ(KeyChordName({Key::kF12}), "F12");
  EXPECT_EQ(KeyChordName({}), "-");
  EXPECT_EQ(ParseKeyChord(" Period "), (KeyChord{Key::kPeriod}));
  EXPECT_EQ(ParseKeyChord("-"), KeyChord{});
  EXPECT_EQ(ParseKeyChord("Hyper+Q"), std::nullopt);
  EXPECT_EQ(ParseKeyChord("F13"), std::nullopt);
  EXPECT_EQ(ParseKeyChord(""), std::nullopt);
}

TEST(KeyChordTest, ConsumeTakeAndSwallow) {
  UiHarness h;
  bool hit = false;
  h.Frame([&](Context &ctx) { hit = ConsumeChord(ctx, {Key::kQ, kModCtrl}); },
          UiHarness::Key(Key::kQ));
  EXPECT_FALSE(hit) << "modifiers must match";
  h.Frame([&](Context &ctx) { hit = ConsumeChord(ctx, {Key::kQ, kModCtrl}); },
          UiHarness::Key(Key::kQ, kModCtrl));
  EXPECT_TRUE(hit);
  h.Frame([&](Context &ctx) { hit = ConsumeChord(ctx, {}); },
          UiHarness::Key(Key::kQ));
  EXPECT_FALSE(hit) << "unbound";
  std::optional<KeyChord> taken;
  h.Frame([&](Context &ctx) { taken = TakeAnyKey(ctx); },
          UiHarness::Key(Key::kF7, kModShift | kModSuper));
  EXPECT_EQ(taken, (KeyChord{Key::kF7, kModShift}));
  bool left = true;
  h.Frame(
      [&](Context &ctx) {
        SwallowKeys(ctx);
        left = ctx.ConsumeKey(Key::kT) || ctx.ConsumeKey(Key::kEnter);
      },
      {InputEvent::KeyDown(Key::kT), InputEvent::KeyDown(Key::kEnter)});
  EXPECT_FALSE(left);
}

TEST(KeyChordTest, CaptureButtonTakesTheNextKeyOrEscape) {
  UiHarness h;
  KeyChord chord{Key::kT};
  KeyCaptureResult r;
  bool escape_left = false;
  auto ui = [&](Context &ctx) {
    r = KeyCaptureButton(ctx, "##trade", chord);
    if (r.captured) {
      chord = *r.captured;
    }
    escape_left = ctx.ConsumeKey(Key::kEscape);
  };
  h.Frame(ui);
  const auto button = [&] {
    h.ctx().PushId(h.ctx().GetId("##trade"));
    const Id id = h.ctx().GetId("T##capture");
    h.ctx().PopId();
    return id;
  };
  const Vec2 at = h.Center(button());
  h.Frame(ui, UiHarness::Click(at));
  EXPECT_TRUE(r.started);
  EXPECT_TRUE(r.waiting);
  // Escape cancels the capture and is not seen after it.
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(r.cancelled);
  EXPECT_FALSE(escape_left);
  EXPECT_EQ(chord, (KeyChord{Key::kT}));
  h.Frame(ui, UiHarness::Click(at));
  ASSERT_TRUE(r.waiting);
  h.Frame(ui, UiHarness::Key(Key::kH, kModCtrl));
  EXPECT_EQ(chord, (KeyChord{Key::kH, kModCtrl}));
  EXPECT_FALSE(r.waiting);
  // Not waiting: Escape is left for others.
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(escape_left);
}

}  // namespace
}  // namespace gloomy::ui
