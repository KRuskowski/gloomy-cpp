#include "gloomy/ui/theme.h"

#include <algorithm>

namespace gloomy::ui {

namespace {

auto Fonts(const text::DefaultFonts &f) -> ThemeFonts {
  return {f.sans, f.bold, f.italic, f.mono};
}

}  // namespace

auto Theme::Dark(const text::DefaultFonts &fonts) -> Theme {
  Theme t;
  t.name = "Dark";
  t.fonts = Fonts(fonts);
  Palette &c = t.colors;
  c.background = Color::Rgb(0x1B1D24);
  c.surface = Color::Rgb(0x23262F);
  c.surface_alt = Color::Rgb(0x272A34);
  c.surface_raised = Color::Rgb(0x2D313C);
  c.primary = Color::Rgb(0x4C8DF6);
  c.on_primary = Color::Rgb(0xFFFFFF);
  c.accent = Color::Rgb(0x3FC1C9);
  c.text = Color::Rgb(0xDDE1E8);
  c.text_muted = Color::Rgb(0x9097A3);
  c.text_disabled = Color::Rgb(0x5C626D);
  c.hover = Color::Rgba(0xFFFFFF14);
  c.active = Color::Rgba(0xFFFFFF24);
  c.focus = Color::Rgb(0x6EA8FF);
  c.selection = Color::Rgba(0x4C8DF64D);
  c.highlight = Color::Rgba(0xF2C94C66);
  c.success = Color::Rgb(0x4CC38A);
  c.warning = Color::Rgb(0xF2A93B);
  c.error = Color::Rgb(0xEF5B5B);
  c.info = Color::Rgb(0x56B6F2);
  c.border = Color::Rgb(0x3A3F4B);
  c.divider = Color::Rgb(0x2F333D);
  c.shadow = Color::Rgba(0x00000080);
  c.scrim = Color::Rgba(0x00000080);
  return t;
}

auto Theme::Light(const text::DefaultFonts &fonts) -> Theme {
  Theme t;
  t.name = "Light";
  t.fonts = Fonts(fonts);
  Palette &c = t.colors;
  c.background = Color::Rgb(0xF4F5F7);
  c.surface = Color::Rgb(0xFFFFFF);
  c.surface_alt = Color::Rgb(0xF7F8FA);
  c.surface_raised = Color::Rgb(0xFFFFFF);
  c.primary = Color::Rgb(0x2F6FDE);
  c.on_primary = Color::Rgb(0xFFFFFF);
  c.accent = Color::Rgb(0x0E9AA7);
  c.text = Color::Rgb(0x1F2329);
  c.text_muted = Color::Rgb(0x5F6672);
  c.text_disabled = Color::Rgb(0xA3A9B3);
  c.hover = Color::Rgba(0x0000000F);
  c.active = Color::Rgba(0x0000001F);
  c.focus = Color::Rgb(0x2F6FDE);
  c.selection = Color::Rgba(0x2F6FDE33);
  c.highlight = Color::Rgba(0xFFD54F99);
  c.success = Color::Rgb(0x23905C);
  c.warning = Color::Rgb(0xC77700);
  c.error = Color::Rgb(0xD03A3A);
  c.info = Color::Rgb(0x1F7DC2);
  c.border = Color::Rgb(0xCDD1D8);
  c.divider = Color::Rgb(0xE3E6EA);
  c.shadow = Color::Rgba(0x00000033);
  c.scrim = Color::Rgba(0x00000059);
  return t;
}

auto Theme::HighContrast(const text::DefaultFonts &fonts) -> Theme {
  Theme t;
  t.name = "High Contrast";
  t.fonts = Fonts(fonts);
  t.metrics.focus_width = 3.0F;
  t.metrics.border_width = 2.0F;
  Palette &c = t.colors;
  c.background = Color::Rgb(0x000000);
  c.surface = Color::Rgb(0x000000);
  c.surface_alt = Color::Rgb(0x0A0A0A);
  c.surface_raised = Color::Rgb(0x000000);
  c.primary = Color::Rgb(0xFFD400);
  c.on_primary = Color::Rgb(0x000000);
  c.accent = Color::Rgb(0x00E5FF);
  c.text = Color::Rgb(0xFFFFFF);
  c.text_muted = Color::Rgb(0xE0E0E0);
  c.text_disabled = Color::Rgb(0x9E9E9E);
  c.hover = Color::Rgba(0xFFFFFF33);
  c.active = Color::Rgba(0xFFFFFF4D);
  c.focus = Color::Rgb(0xFFD400);
  c.selection = Color::Rgba(0xFFD40066);
  c.highlight = Color::Rgba(0x00E5FF80);
  c.success = Color::Rgb(0x00FF7F);
  c.warning = Color::Rgb(0xFFB000);
  c.error = Color::Rgb(0xFF4040);
  c.info = Color::Rgb(0x40C4FF);
  c.border = Color::Rgb(0xFFFFFF);
  c.divider = Color::Rgb(0xBDBDBD);
  c.shadow = Color::Rgba(0x00000000);
  c.scrim = Color::Rgba(0x000000B3);
  return t;
}

auto Theme::Scaled(float factor) const -> Theme {
  const float f = std::clamp(factor, kMinScale, kMaxScale);
  Theme t = *this;
  Metrics &m = t.metrics;
  for (float *v : {&m.font_size,
                   &m.small_font_size,
                   &m.heading_font_size,
                   &m.spacing,
                   &m.padding_x,
                   &m.padding_y,
                   &m.radius,
                   &m.row_height,
                   &m.control_height,
                   &m.scrollbar_width}) {
    *v *= f;
  }
  m.border_width = std::max(1.0F, m.border_width * f);
  m.focus_width = std::max(1.0F, m.focus_width * f);
  return t;
}

auto GlassStyle::On(Surface surface) const -> bool {
  switch (surface) {
    case Surface::kPanel:
      return panels;
    case Surface::kCard:
      return cards;
    case Surface::kMenu:
      return menus;
    case Surface::kDialog:
      return dialogs;
    case Surface::kRadial:
      return radial;
  }
  return false;
}

auto Theme::WithGlass(float surface_alpha) const -> Theme {
  Theme t = *this;
  GlassStyle &g = t.glass;
  g.panels = g.cards = g.menus = g.dialogs = g.radial = true;
  if (g.fallback.a <= 0.0F) {
    g.fallback = colors.surface;
  }
  Palette &c = t.colors;
  c.surface = c.surface.WithAlpha(surface_alpha);
  c.surface_alt = c.surface_alt.WithAlpha(surface_alpha);
  c.surface_raised = c.surface_raised.WithAlpha(surface_alpha);
  return t;
}

}  // namespace gloomy::ui
