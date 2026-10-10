#ifndef GLOOMY_UI_THEME_H_
#define GLOOMY_UI_THEME_H_

#include <string>

#include "gloomy/core/color.h"
#include "gloomy/gfx/draw_list.h"
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
  /// A bar at the left edge of selected rows in grids and lists, so that
  /// the selection reads at a glance and matches an app's selection marks
  /// elsewhere (map brackets); alpha 0 draws none.
  Color selection_accent{0.0F, 0.0F, 0.0F, 0.0F};
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

/// What a surface is painted with (NodeStyle::material).
enum class Material : std::uint8_t {
  /// Its fill colour.
  kSolid,
  /// Frosted glass over the host scene (GlassStyle); its fill colour (or
  /// GlassStyle::fallback) without one.
  kGlass,
};

/// Kinds of surfaces that can be glass, each switched on in GlassStyle.
enum class Surface : std::uint8_t {
  /// BeginPanel() and other framing panels.
  kPanel,
  /// Cards and summary boxes.
  kCard,
  /// Popups, menus, tooltips, the finder.
  kMenu,
  /// Modal dialogs.
  kDialog,
  /// The radial context menu.
  kRadial,
};

/// Frosted glass over a host engine's scene (docs/design.md section 6.5).
/// Off for every surface by default, so a theme looks as it always did;
/// Theme::WithGlass() turns it on. Without a host scene glass falls back
/// to a solid fill whatever these say.
struct GlassStyle {
  /// Blur radius in pixels of the scene (BackdropBlur::LevelsForRadius()).
  float blur_radius = 16.0F;
  /// Mixed over the darkened, blurred scene; the alpha is the opacity.
  Color tint = Color::Rgb(0x14171E).WithAlpha(0.55F);
  /// Brightness taken from the scene under the glass, for text contrast.
  float darken = 0.35F;
  /// Dither in 1/255 steps against banding.
  float noise = 1.5F;
  /// Light rim along the edge (strongest at the top): strength, width px.
  float highlight = 0.08F;
  float highlight_width = 1.5F;
  /// Solid fill for glass without a scene; alpha 0 uses the node's fill.
  Color fallback{0.0F, 0.0F, 0.0F, 0.0F};
  bool panels = false;
  bool cards = false;
  bool menus = false;
  bool dialogs = false;
  bool radial = false;

  /// The look the draw list takes.
  [[nodiscard]] auto Look() const -> gfx::GlassLook {
    return {tint, darken, noise, highlight, highlight_width};
  }

  /// Whether `surface` is glass.
  [[nodiscard]] auto On(Surface surface) const -> bool;
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
  GlassStyle glass;

  /// Dark (default).
  static auto Dark(const text::DefaultFonts &fonts) -> Theme;

  static auto Light(const text::DefaultFonts &fonts) -> Theme;

  /// Black, white and one strong accent; thicker focus rings.
  static auto HighContrast(const text::DefaultFonts &fonts) -> Theme;

  /// The same theme with every metric (font sizes, spacing, padding, radii,
  /// row and control heights) multiplied by `factor`, clamped to
  /// [kMinScale, kMaxScale]: a UI scale setting. Border and focus widths
  /// stay at least one pixel.
  [[nodiscard]] auto Scaled(float factor) const -> Theme;

  /// The same theme for a UI over a host scene: glass on for every
  /// surface kind, and the nested surface colours (surface, surface_alt,
  /// surface_raised) translucent at `surface_alpha`, so that lists and
  /// buttons on glass let it show through. The opaque colours become the
  /// glass fallback.
  [[nodiscard]] auto WithGlass(float surface_alpha = 0.35F) const -> Theme;

  /// The material of a surface of this kind.
  [[nodiscard]] auto MaterialOf(Surface surface) const -> Material {
    return glass.On(surface) ? Material::kGlass : Material::kSolid;
  }

  static constexpr float kMinScale = 0.5F;
  static constexpr float kMaxScale = 3.0F;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_THEME_H_
