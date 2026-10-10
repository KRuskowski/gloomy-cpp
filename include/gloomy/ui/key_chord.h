#ifndef GLOOMY_UI_KEY_CHORD_H_
#define GLOOMY_UI_KEY_CHORD_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/input.h"

namespace gloomy::ui {

// Key chords for rebindable keys (docs/design.md section 9): a key with
// its modifiers, named as scripts and settings files write it ("Ctrl+K",
// "Period", "F5"), matched against this frame's presses, and captured by a
// button ("press the new key") on a settings screen.

/// A key with its modifiers; Key::kNone is unbound.
struct KeyChord {
  Key key = Key::kNone;
  std::uint8_t mods = kModNone;

  auto operator==(const KeyChord &) const -> bool = default;
};

/// The chord's name: modifiers "Ctrl+", "Shift+", "Alt+" then the key
/// ("T", "7", "F5", "Period", "PageDown"); "-" when unbound.
[[nodiscard]] auto KeyChordName(KeyChord chord) -> std::string;

/// Reads KeyChordName()'s text back; nullopt if it names no key ("-" is
/// the unbound chord).
[[nodiscard]] auto ParseKeyChord(std::string_view text)
    -> std::optional<KeyChord>;

/// Takes this frame's press of `chord` (exact modifiers); false for an
/// unbound chord.
auto ConsumeChord(Context &ctx, KeyChord chord) -> bool;

/// Takes the first key pressed this frame, with its modifiers (Super
/// dropped); nullopt if none.
auto TakeAnyKey(Context &ctx) -> std::optional<KeyChord>;

/// Takes every key press and the typed text of this frame, so that what is
/// declared after (a game below a menu) sees none.
void SwallowKeys(Context &ctx);

/// What a key capture button did this frame.
struct KeyCaptureResult {
  /// Clicked: now waiting for a key.
  bool started = false;
  /// The key pressed while waiting; the button stops waiting.
  std::optional<KeyChord> captured;
  /// Escape while waiting: no change.
  bool cancelled = false;
  /// Waiting for a key (after this frame).
  bool waiting = false;
};

/// A button showing `chord`'s name; clicked, it shows "Press a key..." and
/// takes the next key press (Escape cancels, so Escape itself cannot be
/// bound). Declare it before anything that reads Escape this frame.
auto KeyCaptureButton(Context &ctx, std::string_view label, KeyChord chord)
    -> KeyCaptureResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_KEY_CHORD_H_
