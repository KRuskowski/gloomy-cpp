#ifndef GLOOMY_UI_THEME_H_
#define GLOOMY_UI_THEME_H_

#include <string>

#include "gloomy/core/color.h"
#include "gloomy/text/font_set.h"

namespace gloomy::ui {

/// Semantic colours. Widgets never hard-code colours.
struct Palette {
  Color background;
  Color surface;
  Color surface_alt;
  Color surface_raised;
  Color primary;
  Color on_primary;
  Color accent;
  Color text;
  Color text_muted;
  Color text_disabled;
  Color hover;
  Color active;
  Color focus;
  Color selection;
  Color highlight;
  Color success;
  Color warning;
  Color error;
  Color info;
  Color border;
  Color divider;
  Color shadow;
  Color scrim;
};

/// Sizes in pixels.
struct Metrics {
  float font_size = 13.0F;
  float small_font_size = 11.0F;
  float heading_font_size = 16.0F;
  float spacing = 8.0F;
  float padding_x = 8.0F;
  float padding_y = 4.0F;
  float radius = 4.0F;
  float border_width = 1.0F;
  float focus_width = 2.0F;
  float row_height = 22.0F;
  float control_height = 26.0F;
  float scrollbar_width = 10.0F;
};

/// Fonts by role.
struct ThemeFonts {
  text::FontId sans = 0;
  text::FontId bold = 0;
  text::FontId italic = 0;
  text::FontId mono = 0;
};

/// A complete theme: palette, metrics, fonts. See docs/design.md
/// section 13.
struct Theme {
  std::string name;
  Palette colors;
  Metrics metrics;
  ThemeFonts fonts;

  /// Dark (default).
  static auto Dark(const text::DefaultFonts &fonts) -> Theme;

  static auto Light(const text::DefaultFonts &fonts) -> Theme;

  /// Black, white and one strong accent; thicker focus rings.
  static auto HighContrast(const text::DefaultFonts &fonts) -> Theme;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_THEME_H_
