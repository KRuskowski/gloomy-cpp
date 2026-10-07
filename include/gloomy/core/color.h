#ifndef GLOOMY_CORE_COLOR_H_
#define GLOOMY_CORE_COLOR_H_

#include <algorithm>
#include <cstdint>

namespace gloomy {

/// A straight (non-premultiplied) RGBA colour with components in [0, 1].
struct Color {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;

  /// Builds a colour from 0xRRGGBB, fully opaque.
  static constexpr auto Rgb(std::uint32_t rgb) -> Color {
    return {static_cast<float>((rgb >> 16U) & 0xFFU) / 255.0F,
            static_cast<float>((rgb >> 8U) & 0xFFU) / 255.0F,
            static_cast<float>(rgb & 0xFFU) / 255.0F,
            1.0F};
  }

  /// Builds a colour from 0xRRGGBBAA.
  static constexpr auto Rgba(std::uint32_t rgba) -> Color {
    Color c = Rgb(rgba >> 8U);
    c.a = static_cast<float>(rgba & 0xFFU) / 255.0F;
    return c;
  }

  /// The same colour with alpha replaced.
  constexpr auto WithAlpha(float alpha) const -> Color {
    return {r, g, b, alpha};
  }

  /// Packs to 8-bit RGBA with red in the lowest byte, the layout GLSL's
  /// unpackUnorm4x8 expects.
  constexpr auto Pack() const -> std::uint32_t {
    auto q = [](float v) -> std::uint32_t {
      return static_cast<std::uint32_t>(std::clamp(v, 0.0F, 1.0F) * 255.0F +
                                        0.5F);
    };
    return q(r) | (q(g) << 8U) | (q(b) << 16U) | (q(a) << 24U);
  }

  friend constexpr auto operator==(const Color &, const Color &)
      -> bool = default;
};

/// Linear interpolation between two colours, component-wise.
constexpr auto Mix(const Color &a, const Color &b, float t) -> Color {
  return {a.r + (b.r - a.r) * t,
          a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t,
          a.a + (b.a - a.a) * t};
}

}  // namespace gloomy

#endif  // GLOOMY_CORE_COLOR_H_
