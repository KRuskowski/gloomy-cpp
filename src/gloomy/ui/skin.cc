#include "gloomy/ui/skin.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>

#include "gloomy/ui/hud.h"

namespace gloomy::ui {

namespace {

constexpr float kStampAlpha = 0.85F;
constexpr float kStampBorder = 2.5F;
constexpr float kStampPadX = 10.0F;
constexpr float kStampPadY = 4.0F;

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

auto ParseFloat(std::string_view s, float &out) -> bool {
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc() && end == s.data() + s.size();
}

/// Capitals of ASCII letters, the rest unchanged.
auto Upper(std::string_view s) -> std::string {
  std::string out(s);
  for (char &c : out) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
  }
  return out;
}

/// A rosette: a ring with petals, for certificate corners.
void Rosette(gfx::DrawList &list, Vec2 c, float r, const Color &color) {
  list.StrokeCircle(c, r, color, 1.2F);
  list.FillCircle(c, r * 0.3F, color);
  constexpr int kPetals = 8;
  for (int i = 0; i < kPetals; ++i) {
    const float a = static_cast<float>(i) * 2.0F * std::numbers::pi_v<float> /
                    static_cast<float>(kPetals);
    list.StrokeCircle(ArcPoint(c, r * 0.62F, a), r * 0.28F, color, 1.0F);
  }
}

/// One side of an ornate band: from `a` to `b` (a straight edge), the
/// band reaching `depth` towards `inward`.
void OrnamentSide(gfx::DrawList &list, Vec2 a, Vec2 b, Vec2 inward, float depth,
                  Ornament ornament, const Color &color) {
  const float len = std::hypot(b.x - a.x, b.y - a.y);
  if (len < 1.0F) {
    return;
  }
  const Vec2 d{(b.x - a.x) / len, (b.y - a.y) / len};
  const float step = std::max(depth * 1.1F, 6.0F);
  const int count = std::max(1, static_cast<int>(len / step));
  const float pitch = len / static_cast<float>(count);
  const Vec2 mid = inward * (depth * 0.5F);
  auto along = [&](float t) -> Vec2 {
    return a + d * t;
  };
  switch (ornament) {
    case Ornament::kRings:
      for (int i = 0; i < count; ++i) {
        const Vec2 c = along((static_cast<float>(i) + 0.5F) * pitch) + mid;
        list.StrokeCircle(c, depth * 0.36F, color, 1.0F);
      }
      break;
    case Ornament::kChevrons:
      for (int i = 0; i < count; ++i) {
        const float t0 = static_cast<float>(i) * pitch;
        const Vec2 p0 = along(t0 + pitch * 0.15F) + inward * (depth * 0.2F);
        const Vec2 p1 = along(t0 + pitch * 0.5F) + inward * (depth * 0.8F);
        const Vec2 p2 = along(t0 + pitch * 0.85F) + inward * (depth * 0.2F);
        list.Line(p0, p1, 1.2F, color);
        list.Line(p1, p2, 1.2F, color);
      }
      break;
    case Ornament::kDiamonds:
      list.Line(a + inward * 0.5F, b + inward * 0.5F, 1.0F, color);
      list.Line(a + inward * (depth - 0.5F),
                b + inward * (depth - 0.5F),
                1.0F,
                color);
      for (int i = 0; i < count; ++i) {
        const Vec2 c = along((static_cast<float>(i) + 0.5F) * pitch) + mid;
        const float h = depth * 0.32F;
        const Vec2 n = d * h;
        const Vec2 m = inward * h;
        list.Line(c - n, c - m, 1.0F, color);
        list.Line(c - m, c + n, 1.0F, color);
        list.Line(c + n, c + m, 1.0F, color);
        list.Line(c + m, c - n, 1.0F, color);
      }
      break;
    case Ornament::kWave:
    case Ornament::kCount: {
      list.Line(a + inward * 0.5F, b + inward * 0.5F, 1.0F, color);
      list.Line(a + inward * (depth - 0.5F),
                b + inward * (depth - 0.5F),
                1.0F,
                color);
      const int steps = count * 8;
      Vec2 prev = along(0.0F) + mid;
      for (int i = 1; i <= steps; ++i) {
        const float t = len * static_cast<float>(i) / static_cast<float>(steps);
        const float s = std::sin(t / pitch * 2.0F * std::numbers::pi_v<float>);
        const Vec2 p = along(t) + mid + inward * (s * depth * 0.3F);
        list.Line(prev, p, 1.1F, color);
        prev = p;
      }
      break;
    }
  }
}

}  // namespace

auto PanelSkin::ToneInk(Tone tone) const -> Color {
  switch (tone) {
    case Tone::kGood:
      return good;
    case Tone::kWarning:
      return warning;
    case Tone::kBad:
      return bad;
    case Tone::kNeutral:
      break;
  }
  return text;
}

auto PanelSkin::FromTheme(const Theme &theme) -> PanelSkin {
  const Palette &c = theme.colors;
  PanelSkin s;
  s.name = theme.name;
  s.ink = c.accent;
  s.ink_muted = c.text_muted;
  s.paper = Mix(c.background, c.surface, 0.5F).WithAlpha(1.0F);
  s.paper_edge = c.surface_raised.WithAlpha(1.0F);
  s.text = c.text;
  s.text_muted = c.text_muted;
  s.good = c.success;
  s.warning = c.warning;
  s.bad = c.error;
  s.friendly = c.info;
  s.neutral = c.text;
  s.hostile = c.error;
  s.frame = std::max(2.0F, theme.metrics.border_width * 3.0F);
  s.inset = theme.metrics.border_width;
  s.corner = theme.metrics.radius;
  return s;
}

auto PanelSkin::OnPaper() const -> PanelSkin {
  PanelSkin s = *this;
  const Color dark = Color::Rgb(0x1E2228);
  s.paper = Color::Rgb(0xEFE6D0);
  s.paper_edge = Color::Rgb(0xDCCFAE);
  s.text = dark;
  s.text_muted = Color::Rgb(0x5E5A50);
  s.ink = Mix(ink, dark, 0.55F);
  s.ink_muted = Color::Rgb(0x8A8270);
  s.good = Mix(good, dark, 0.45F);
  s.warning = Mix(warning, dark, 0.4F);
  s.bad = Mix(bad, dark, 0.3F);
  s.friendly = Mix(friendly, dark, 0.45F);
  s.neutral = Mix(neutral, dark, 0.6F);
  s.hostile = Mix(hostile, dark, 0.3F);
  s.glow = 0.0F;
  return s;
}

auto ParseHexColor(std::string_view text, Color &out) -> bool {
  if (text.size() != 7 && text.size() != 9) {
    return false;
  }
  if (text.front() != '#') {
    return false;
  }
  std::uint32_t v = 0;
  const auto [end, ec] =
      std::from_chars(text.data() + 1, text.data() + text.size(), v, 16);
  if (ec != std::errc() || end != text.data() + text.size()) {
    return false;
  }
  out = text.size() == 7 ? Color::Rgb(v) : Color::Rgba(v);
  return true;
}

auto ParsePanelSkin(std::string_view text, const PanelSkin &base,
                    std::vector<std::string> *errors) -> PanelSkin {
  PanelSkin s = base;
  auto report = [&](int line, std::string_view key, std::string_view value) {
    if (errors != nullptr) {
      errors->push_back("line " + std::to_string(line) + ": bad value '" +
                        std::string(value) + "' for " + std::string(key));
    }
  };
  int number = 0;
  while (!text.empty()) {
    const std::size_t nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
    ++number;
    if (const std::size_t hash = line.find("# ");
        hash != 0 && hash != std::string_view::npos) {
      line = line.substr(0, hash);
    }
    line = Trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const std::size_t eq = line.find('=');
    if (eq == std::string_view::npos) {
      continue;
    }
    const std::string_view key = Trim(line.substr(0, eq));
    const std::string_view value = Trim(line.substr(eq + 1));
    Color *color = nullptr;
    float *number_out = nullptr;
    float scale = 1.0F;
    if (key == "name") {
      s.name = std::string(value);
      continue;
    }
    if (key == "primary") {
      color = &s.ink;
    } else if (key == "secondary") {
      color = &s.ink_muted;
    } else if (key == "own") {
      color = &s.good;
    } else if (key == "friendly") {
      color = &s.friendly;
    } else if (key == "neutral") {
      color = &s.neutral;
    } else if (key == "hostile") {
      color = &s.hostile;
    } else if (key == "warning") {
      color = &s.warning;
    } else if (key == "alert") {
      color = &s.bad;
    } else if (key == "panel_paper") {
      color = &s.paper;
    } else if (key == "panel_paper_edge") {
      color = &s.paper_edge;
    } else if (key == "panel_text") {
      color = &s.text;
    } else if (key == "panel_text_muted") {
      color = &s.text_muted;
    } else if (key == "line") {
      number_out = &s.frame;
      scale = 2.0F;
    } else if (key == "glow") {
      number_out = &s.glow;
    } else if (key == "panel_frame") {
      number_out = &s.frame;
    } else if (key == "panel_inset") {
      number_out = &s.inset;
    } else if (key == "panel_corner") {
      number_out = &s.corner;
    } else if (key == "panel_bolt") {
      number_out = &s.bolt;
    } else {
      continue;
    }
    if (color != nullptr) {
      Color c;
      if (ParseHexColor(value, c)) {
        *color = c;
      } else {
        report(number, key, value);
      }
    } else {
      float f = 0.0F;
      if (ParseFloat(value, f) && f >= 0.0F) {
        *number_out = f * scale;
      } else {
        report(number, key, value);
      }
    }
  }
  return s;
}

void PaintFrame(gfx::DrawList &list, const Rect &r, const PanelSkin &skin,
                const Color &fill, const Color &edge) {
  const auto radii = gfx::CornerRadii::All(skin.corner);
  list.FillRect(r, fill, radii);
  if (skin.frame > 0.0F) {
    list.StrokeRect(r, edge, skin.frame, radii);
  }
  if (skin.inset > 0.0F) {
    const float in = skin.frame + 2.0F;
    list.StrokeRect(r.Inset(in),
                    edge.WithAlpha(edge.a * 0.35F),
                    skin.inset,
                    gfx::CornerRadii::All(std::max(0.0F, skin.corner - 1.0F)));
  }
  if (skin.bolt > 0.0F && r.Width() > 40.0F && r.Height() > 30.0F) {
    const float o = skin.frame + 5.0F;
    const Color bolt = edge.WithAlpha(edge.a * 0.8F);
    list.FillCircle({r.x0 + o, r.y0 + o}, skin.bolt, bolt);
    list.FillCircle({r.x1 - o, r.y0 + o}, skin.bolt, bolt);
    list.FillCircle({r.x0 + o, r.y1 - o}, skin.bolt, bolt);
    list.FillCircle({r.x1 - o, r.y1 - o}, skin.bolt, bolt);
  }
}

auto StampSize(text::TextSystem &text, text::FontId bold, float size,
               std::string_view label) -> Vec2 {
  const float w = text.Measure(Upper(label), bold, size);
  return {w + 2.0F * kStampPadX + 2.0F * kStampBorder,
          size * 1.3F + 2.0F * kStampPadY + 2.0F * kStampBorder};
}

auto PaintStamp(gfx::DrawList &list, text::TextSystem &text, text::FontId bold,
                float size, Vec2 centre, std::string_view label,
                const Color &color) -> Rect {
  const Vec2 sz = StampSize(text, bold, size, label);
  const Rect r{centre.x - sz.x * 0.5F,
               centre.y - sz.y * 0.5F,
               centre.x + sz.x * 0.5F,
               centre.y + sz.y * 0.5F};
  const Color c = color.WithAlpha(color.a * kStampAlpha);
  list.StrokeRect(r, c, kStampBorder, gfx::CornerRadii::All(4.0F));
  list.StrokeRect(r.Inset(kStampBorder + 1.5F),
                  c.WithAlpha(c.a * 0.7F),
                  1.0F,
                  gfx::CornerRadii::All(2.0F));
  text.DrawFitted(list,
                  Upper(label),
                  bold,
                  size,
                  r.Inset(kStampBorder),
                  c,
                  text::HAlign::kCenter);
  return r;
}

void PaintSeal(gfx::DrawList &list, Vec2 centre, float radius,
               const Color &color, int points) {
  list.StrokeCircle(centre, radius * 0.78F, color, 1.5F);
  list.StrokeCircle(centre, radius * 0.6F, color, 1.0F);
  list.FillCircle(centre, radius * 0.18F, color);
  for (int i = 0; i < points; ++i) {
    const float a = static_cast<float>(i) * 2.0F * std::numbers::pi_v<float> /
                    static_cast<float>(points);
    list.Line(ArcPoint(centre, radius * 0.84F, a),
              ArcPoint(centre, radius, a),
              1.5F,
              color);
  }
}

auto OrnamentOf(std::uint32_t seed) -> Ornament {
  // Mixed so that neighbouring ids (issuers 3 and 4) differ.
  std::uint32_t h = seed + 0x9E3779B9U;
  h ^= h >> 16U;
  h *= 0x85EBCA6BU;
  h ^= h >> 13U;
  h *= 0xC2B2AE35U;
  h ^= h >> 16U;
  return static_cast<Ornament>(h %
                               static_cast<std::uint32_t>(Ornament::kCount));
}

void PaintOrnateBorder(gfx::DrawList &list, const Rect &r, Ornament ornament,
                       float band, const Color &color) {
  list.StrokeRect(r, color, 1.5F);
  const Rect in = r.Inset(band);
  list.StrokeRect(in, color, 1.0F);
  const float d = band - 4.0F;
  const float o = 2.0F;
  const float c = band;
  // Each side runs between the corner rosettes.
  OrnamentSide(list,
               {r.x0 + c, r.y0 + o},
               {r.x1 - c, r.y0 + o},
               {0, 1},
               d,
               ornament,
               color);
  OrnamentSide(list,
               {r.x0 + c, r.y1 - o},
               {r.x1 - c, r.y1 - o},
               {0, -1},
               d,
               ornament,
               color);
  OrnamentSide(list,
               {r.x0 + o, r.y0 + c},
               {r.x0 + o, r.y1 - c},
               {1, 0},
               d,
               ornament,
               color);
  OrnamentSide(list,
               {r.x1 - o, r.y0 + c},
               {r.x1 - o, r.y1 - c},
               {-1, 0},
               d,
               ornament,
               color);
  const float rr = band * 0.42F;
  Rosette(list, {r.x0 + band * 0.5F, r.y0 + band * 0.5F}, rr, color);
  Rosette(list, {r.x1 - band * 0.5F, r.y0 + band * 0.5F}, rr, color);
  Rosette(list, {r.x0 + band * 0.5F, r.y1 - band * 0.5F}, rr, color);
  Rosette(list, {r.x1 - band * 0.5F, r.y1 - band * 0.5F}, rr, color);
}

void PaintHatch(gfx::DrawList &list, const Rect &r, const Color &color,
                float spacing, float width) {
  if (r.Empty() || spacing <= 0.0F) {
    return;
  }
  list.PushClip(r);
  const float h = r.Height();
  for (float x = r.x0 - h; x < r.x1; x += spacing) {
    list.Line({x, r.y1}, {x + h, r.y0}, width, color);
  }
  list.PopClip();
}

void PaintSegments(gfx::DrawList &list, const Rect &r, int segments, int filled,
                   int damaged, const Color &on, const Color &damaged_color,
                   const Color &off, bool vertical) {
  if (segments <= 0 || r.Empty()) {
    return;
  }
  const float gap = 2.0F;
  const float length = vertical ? r.Height() : r.Width();
  const float seg = (length - gap * static_cast<float>(segments - 1)) /
                    static_cast<float>(segments);
  if (seg <= 0.5F) {
    return;
  }
  filled = std::clamp(filled, 0, segments);
  damaged = std::clamp(damaged, 0, segments - filled);
  for (int i = 0; i < segments; ++i) {
    const float a = static_cast<float>(i) * (seg + gap);
    const Rect s = vertical ? Rect{r.x0, r.y1 - a - seg, r.x1, r.y1 - a}
                            : Rect{r.x0 + a, r.y0, r.x0 + a + seg, r.y1};
    if (i < filled) {
      list.FillRect(s, on);
    } else if (i < filled + damaged) {
      list.FillRect(s, damaged_color.WithAlpha(damaged_color.a * 0.35F));
      list.StrokeRect(s, damaged_color, 1.0F);
      list.Line(
          {s.x0 + 1, s.y0 + 1}, {s.x1 - 1, s.y1 - 1}, 1.0F, damaged_color);
    } else {
      list.StrokeRect(s, off, 1.0F);
    }
  }
}

void StrokeEllipse(gfx::DrawList &list, const Rect &box, float width,
                   const Color &color, int segments) {
  const Vec2 c = box.Center();
  const float rx = box.Width() * 0.5F;
  const float ry = box.Height() * 0.5F;
  segments = std::max(8, segments);
  Vec2 prev{c.x + rx, c.y};
  for (int i = 1; i <= segments; ++i) {
    const float a = static_cast<float>(i) * 2.0F * std::numbers::pi_v<float> /
                    static_cast<float>(segments);
    const Vec2 p{c.x + rx * std::cos(a), c.y + ry * std::sin(a)};
    list.Line(prev, p, width, color);
    prev = p;
  }
}

}  // namespace gloomy::ui
