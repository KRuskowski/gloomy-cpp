#ifndef GLOOMY_UI_INPUT_H_
#define GLOOMY_UI_INPUT_H_

#include <cstdint>
#include <string>

#include "gloomy/core/geometry.h"

namespace gloomy::ui {

/// Platform-neutral key codes. Letters and digits are contiguous.
enum class Key : std::uint16_t {
  kNone,
  kTab,
  kEnter,
  kEscape,
  kBackspace,
  kDelete,
  kInsert,
  kLeft,
  kRight,
  kUp,
  kDown,
  kHome,
  kEnd,
  kPageUp,
  kPageDown,
  kSpace,
  kSlash,
  kPeriod,
  kComma,
  kMinus,
  /// The key with '=' and '+' on US layouts; also keypad plus.
  kEquals,
  kA,
  kB,
  kC,
  kD,
  kE,
  kF,
  kG,
  kH,
  kI,
  kJ,
  kK,
  kL,
  kM,
  kN,
  kO,
  kP,
  kQ,
  kR,
  kS,
  kT,
  kU,
  kV,
  kW,
  kX,
  kY,
  kZ,
  k0,
  k1,
  k2,
  k3,
  k4,
  k5,
  k6,
  k7,
  k8,
  k9,
  kF1,
  kF2,
  kF3,
  kF4,
  kF5,
  kF6,
  kF7,
  kF8,
  kF9,
  kF10,
  kF11,
  kF12,
};

/// Modifier key bits.
enum Mod : std::uint8_t {
  kModNone = 0,
  kModShift = 1,
  kModCtrl = 2,
  kModAlt = 4,
  kModSuper = 8,
};

enum class PointerButton : std::uint8_t { kLeft, kRight, kMiddle };

/// One input event, translated by the platform layer.
struct InputEvent {
  enum class Type : std::uint8_t {
    kPointerMove,
    kPointerDown,
    kPointerUp,
    kWheel,
    kKeyDown,
    kKeyUp,
    /// Committed text (after keyboard layout and IME).
    kText,
  };

  Type type = Type::kPointerMove;
  Vec2 pos;
  PointerButton button = PointerButton::kLeft;
  /// Wheel movement in lines; positive y scrolls content up (wheel away).
  Vec2 wheel;
  Key key = Key::kNone;
  std::uint8_t mods = kModNone;
  bool repeat = false;
  std::string text;

  static auto Move(float x, float y) -> InputEvent {
    InputEvent e;
    e.type = Type::kPointerMove;
    e.pos = {x, y};
    return e;
  }

  static auto Down(float x, float y, PointerButton b = PointerButton::kLeft,
                   std::uint8_t m = kModNone) -> InputEvent {
    InputEvent e = Move(x, y);
    e.type = Type::kPointerDown;
    e.button = b;
    e.mods = m;
    return e;
  }

  static auto Up(float x, float y, PointerButton b = PointerButton::kLeft)
      -> InputEvent {
    InputEvent e = Move(x, y);
    e.type = Type::kPointerUp;
    e.button = b;
    return e;
  }

  static auto Wheel(float dx, float dy) -> InputEvent {
    InputEvent e;
    e.type = Type::kWheel;
    e.wheel = {dx, dy};
    return e;
  }

  static auto KeyDown(Key k, std::uint8_t m = kModNone) -> InputEvent {
    InputEvent e;
    e.type = Type::kKeyDown;
    e.key = k;
    e.mods = m;
    return e;
  }

  static auto Text(std::string t) -> InputEvent {
    InputEvent e;
    e.type = Type::kText;
    e.text = std::move(t);
    return e;
  }
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_INPUT_H_
