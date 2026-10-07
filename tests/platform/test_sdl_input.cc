#include <SDL.h>
#include <gtest/gtest.h>

#include "gloomy/platform/sdl_window.h"

namespace gloomy::platform {
namespace {

TEST(SdlInputTest, MouseIsScaledToPixels) {
  SDL_Event e{};
  e.type = SDL_MOUSEBUTTONDOWN;
  e.button.button = SDL_BUTTON_RIGHT;
  e.button.x = 10;
  e.button.y = 20;
  const auto ev = TranslateEvent(e, 2.0F);
  ASSERT_TRUE(ev.has_value());
  EXPECT_EQ(ev->type, ui::InputEvent::Type::kPointerDown);
  EXPECT_EQ(ev->button, ui::PointerButton::kRight);
  EXPECT_EQ(ev->pos, (Vec2{20, 40}));
}

TEST(SdlInputTest, KeysAndModifiers) {
  SDL_Event e{};
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = SDLK_c;
  e.key.keysym.mod = KMOD_LCTRL | KMOD_LSHIFT;
  const auto ev = TranslateEvent(e, 1.0F);
  ASSERT_TRUE(ev.has_value());
  EXPECT_EQ(ev->key, ui::Key::kC);
  EXPECT_EQ(ev->mods, ui::kModCtrl | ui::kModShift);

  e.key.keysym.sym = SDLK_F3;
  e.key.keysym.mod = 0;
  EXPECT_EQ(TranslateEvent(e, 1.0F)->key, ui::Key::kF3);
  e.key.keysym.sym = SDLK_PAGEDOWN;
  EXPECT_EQ(TranslateEvent(e, 1.0F)->key, ui::Key::kPageDown);
  e.key.keysym.sym = SDLK_CAPSLOCK;
  EXPECT_FALSE(TranslateEvent(e, 1.0F).has_value());
}

TEST(SdlInputTest, TextAndWheel) {
  SDL_Event e{};
  e.type = SDL_TEXTINPUT;
  std::snprintf(e.text.text, sizeof(e.text.text), "%s", "\xC3\xA4");
  EXPECT_EQ(TranslateEvent(e, 1.0F)->text, "\xC3\xA4");
  SDL_Event w{};
  w.type = SDL_MOUSEWHEEL;
  w.wheel.preciseY = -1.5F;
  w.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
  EXPECT_FLOAT_EQ(TranslateEvent(w, 1.0F)->wheel.y, 1.5F);
}

}  // namespace
}  // namespace gloomy::platform
