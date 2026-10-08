#ifndef GLOOMY_ICON_ICON_SET_H_
#define GLOOMY_ICON_ICON_SET_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gloomy::icon {

/// Index of an icon in an IconSet.
using IconId = std::uint32_t;

/// No icon.
inline constexpr IconId kNoIcon = 0xFFFFFFFFU;

/// An 8-bit coverage image, row by row, `width` bytes per row.
struct IconImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;

  [[nodiscard]] auto At(int x, int y) const -> std::uint8_t {
    return pixels[static_cast<std::size_t>(y) * width + x];
  }

  [[nodiscard]] auto Empty() const -> bool { return width == 0; }
};

/// Vector icons parsed from SVG (nanosvg: paths, basic shapes, fills and
/// strokes; no text, gradients are flattened to coverage). Icons are masks:
/// only their coverage counts, the colour comes from whoever draws them, so
/// one icon serves every relation colour. Author them on a square view box
/// (e.g. `viewBox="0 0 24 24"`); they are scaled to the requested pixel
/// size. Graphics independent and testable without a device; IconCache
/// puts the rasterised icons into the atlas.
class IconSet {
 public:
  IconSet();
  IconSet(const IconSet &) = delete;
  auto operator=(const IconSet &) -> IconSet & = delete;
  IconSet(IconSet &&) noexcept;
  auto operator=(IconSet &&) noexcept -> IconSet &;
  ~IconSet();

  /// Parses `svg` and adds it as `name`; an icon of the same name is
  /// replaced and keeps its id.
  /// @return kNoIcon if the SVG has no shapes or no size
  auto Add(std::string_view name, std::string_view svg) -> IconId;

  /// @return kNoIcon if there is no icon of that name
  [[nodiscard]] auto Find(std::string_view name) const -> IconId;

  [[nodiscard]] auto Name(IconId id) const -> const std::string &;

  /// Rasterises icon `id` to a `px` x `px` coverage image, shifted right
  /// and down by `dx`, `dy` pixels (subpixel placement). A non-square view
  /// box is fitted and centred.
  /// @return an empty image for kNoIcon or px < 1
  [[nodiscard]] auto Rasterise(IconId id, int px, float dx = 0.0F,
                               float dy = 0.0F) const -> IconImage;

  [[nodiscard]] auto size() const -> std::size_t;

  /// Bumped whenever an icon is replaced, so caches can tell.
  [[nodiscard]] auto version() const -> std::uint64_t { return version_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::uint64_t version_ = 0;
};

/// Grows coverage by `radius` pixels in every direction (a maximum filter
/// over a disc), for halos that keep icons readable over lines and other
/// icons. The result is `2 * radius` pixels wider and higher.
[[nodiscard]] auto Dilate(const IconImage &image, int radius) -> IconImage;

}  // namespace gloomy::icon

#endif  // GLOOMY_ICON_ICON_SET_H_
