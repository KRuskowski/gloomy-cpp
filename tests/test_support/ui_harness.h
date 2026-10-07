#ifndef GLOOMY_TESTS_TEST_SUPPORT_UI_HARNESS_H_
#define GLOOMY_TESTS_TEST_SUPPORT_UI_HARNESS_H_

#include <functional>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/input.h"
#include "gloomy/ui/theme.h"

namespace gloomy::testing {

/// A headless UI: fonts, text system and context, driven frame by frame
/// with synthetic input. No GPU needed.
class UiHarness {
 public:
  explicit UiHarness(Vec2 viewport = {640, 480})
      : ids_(fonts_.AddDefaultFonts({})),
        text_(fonts_, atlas_),
        ctx_(text_, ui::Theme::Dark(ids_)),
        viewport_(viewport) {}

  /// Runs one frame with `events`, declaring the UI with `build`. Time
  /// advances by 1/60 s per frame.
  void Frame(const std::function<void(ui::Context &)> &build,
             std::vector<ui::InputEvent> events = {}) {
    time_ += 1.0 / 60.0;
    ctx_.BeginFrame(viewport_, events, time_);
    build(ctx_);
    ctx_.EndFrame();
  }

  /// Centre of node `id` in the last frame.
  auto Center(Id id) const -> Vec2 {
    return ctx_.LastRect(id).value_or(Rect{}).Center();
  }

  /// Events for a click at `p`.
  static auto Click(Vec2 p) -> std::vector<ui::InputEvent> {
    return {ui::InputEvent::Move(p.x, p.y),
            ui::InputEvent::Down(p.x, p.y),
            ui::InputEvent::Up(p.x, p.y)};
  }

  static auto Key(ui::Key k, std::uint8_t mods = ui::kModNone)
      -> std::vector<ui::InputEvent> {
    return {ui::InputEvent::KeyDown(k, mods)};
  }

  void AdvanceTime(double seconds) { time_ += seconds; }

  auto ctx() -> ui::Context & { return ctx_; }

  auto text() -> text::TextSystem & { return text_; }

  auto fonts() -> const text::DefaultFonts & { return ids_; }

  auto atlas() -> Atlas & { return atlas_; }

 private:
  text::FontSet fonts_;
  text::DefaultFonts ids_;
  Atlas atlas_{1024, 4};
  text::TextSystem text_;
  ui::Context ctx_;
  Vec2 viewport_;
  double time_ = 1.0;
};

}  // namespace gloomy::testing

#endif  // GLOOMY_TESTS_TEST_SUPPORT_UI_HARNESS_H_
