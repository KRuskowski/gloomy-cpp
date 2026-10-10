#include "gloomy/ui/key_chord.h"

#include <charconv>
#include <string>

#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

struct NamedKey {
  std::string_view name;
  Key key;
};

constexpr NamedKey kKeyNames[] = {
    {"Tab", Key::kTab},       {"Enter", Key::kEnter},
    {"Escape", Key::kEscape}, {"Backspace", Key::kBackspace},
    {"Delete", Key::kDelete}, {"Insert", Key::kInsert},
    {"Left", Key::kLeft},     {"Right", Key::kRight},
    {"Up", Key::kUp},         {"Down", Key::kDown},
    {"Home", Key::kHome},     {"End", Key::kEnd},
    {"PageUp", Key::kPageUp}, {"PageDown", Key::kPageDown},
    {"Space", Key::kSpace},   {"Slash", Key::kSlash},
    {"Period", Key::kPeriod}, {"Comma", Key::kComma},
    {"Minus", Key::kMinus},   {"Equals", Key::kEquals},
};

auto InRange(Key key, Key first, Key last) -> bool {
  return static_cast<int>(key) >= static_cast<int>(first) &&
         static_cast<int>(key) <= static_cast<int>(last);
}

auto Offset(Key key, Key first) -> int {
  return static_cast<int>(key) - static_cast<int>(first);
}

auto BaseKeyName(Key key) -> std::string {
  for (const NamedKey &n : kKeyNames) {
    if (n.key == key) {
      return std::string(n.name);
    }
  }
  if (InRange(key, Key::kA, Key::kZ)) {
    return std::string(1, static_cast<char>('A' + Offset(key, Key::kA)));
  }
  if (InRange(key, Key::k0, Key::k9)) {
    return std::string(1, static_cast<char>('0' + Offset(key, Key::k0)));
  }
  if (InRange(key, Key::kF1, Key::kF12)) {
    return "F" + std::to_string(Offset(key, Key::kF1) + 1);
  }
  return "";
}

auto ParseBaseKey(std::string_view name) -> Key {
  for (const NamedKey &n : kKeyNames) {
    if (n.name == name) {
      return n.key;
    }
  }
  if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
    return static_cast<Key>(static_cast<int>(Key::kA) + (name[0] - 'A'));
  }
  if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
    return static_cast<Key>(static_cast<int>(Key::k0) + (name[0] - '0'));
  }
  if (name.size() >= 2 && name[0] == 'F') {
    int n = 0;
    const char *end = name.data() + name.size();
    const auto [p, ec] = std::from_chars(name.data() + 1, end, n);
    if (ec == std::errc() && p == end && n >= 1 && n <= 12) {
      return static_cast<Key>(static_cast<int>(Key::kF1) + n - 1);
    }
  }
  return Key::kNone;
}

auto Trim(std::string_view s) -> std::string_view {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return s;
}

}  // namespace

auto KeyChordName(KeyChord chord) -> std::string {
  const std::string base = BaseKeyName(chord.key);
  if (base.empty()) {
    return "-";
  }
  std::string out;
  if ((chord.mods & kModCtrl) != 0) {
    out += "Ctrl+";
  }
  if ((chord.mods & kModShift) != 0) {
    out += "Shift+";
  }
  if ((chord.mods & kModAlt) != 0) {
    out += "Alt+";
  }
  return out + base;
}

auto ParseKeyChord(std::string_view text) -> std::optional<KeyChord> {
  text = Trim(text);
  if (text == "-") {
    return KeyChord{};
  }
  KeyChord chord;
  for (;;) {
    if (text.starts_with("Ctrl+")) {
      chord.mods |= kModCtrl;
      text.remove_prefix(5);
    } else if (text.starts_with("Shift+")) {
      chord.mods |= kModShift;
      text.remove_prefix(6);
    } else if (text.starts_with("Alt+")) {
      chord.mods |= kModAlt;
      text.remove_prefix(4);
    } else {
      break;
    }
  }
  chord.key = ParseBaseKey(text);
  if (chord.key == Key::kNone) {
    return std::nullopt;
  }
  return chord;
}

auto ConsumeChord(Context &ctx, KeyChord chord) -> bool {
  return chord.key != Key::kNone && ctx.ConsumeKey(chord.key, chord.mods);
}

auto TakeAnyKey(Context &ctx) -> std::optional<KeyChord> {
  for (std::size_t k = 1; k < kKeyCount; ++k) {
    std::uint8_t mods = 0;
    if (ctx.ConsumeKeyAnyMods(static_cast<Key>(k), mods)) {
      // Super is no part of a binding (the desktop takes it).
      return KeyChord{static_cast<Key>(k),
                      static_cast<std::uint8_t>(mods & ~kModSuper)};
    }
  }
  return std::nullopt;
}

void SwallowKeys(Context &ctx) {
  while (TakeAnyKey(ctx)) {}
  ctx.TakeText();
}

auto KeyCaptureButton(Context &ctx, std::string_view label, KeyChord chord)
    -> KeyCaptureResult {
  KeyCaptureResult result;
  const Id id = ctx.GetId(label);
  bool &waiting = ctx.State<bool>(id);
  if (waiting) {
    if (ctx.ConsumeKey(Key::kEscape)) {
      waiting = false;
      result.cancelled = true;
    } else if (const std::optional<KeyChord> key = TakeAnyKey(ctx)) {
      waiting = false;
      result.captured = key;
    }
  }
  ctx.PushId(id);
  const std::string text = waiting ? std::string("Press a key...##capture")
                                   : KeyChordName(chord) + "##capture";
  if (Button(ctx, text, waiting ? ButtonKind::kPrimary : ButtonKind::kNormal) &&
      !waiting) {
    waiting = true;
    result.started = true;
  }
  ctx.PopId();
  result.waiting = waiting;
  return result;
}

}  // namespace gloomy::ui
