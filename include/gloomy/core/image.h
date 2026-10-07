#ifndef GLOOMY_CORE_IMAGE_H_
#define GLOOMY_CORE_IMAGE_H_

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace gloomy {

/// An 8-bit RGBA image in row-major order without padding.
struct Image {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;

  /// Creates a zero-filled image.
  static auto Make(int w, int h) -> Image;

  /// The four channels of pixel (x, y).
  auto At(int x, int y) -> std::uint8_t * {
    return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
  }

  auto At(int x, int y) const -> const std::uint8_t * {
    return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
  }
};

/// Writes `image` as an 8-bit RGBA PNG.
/// @return An error message on failure.
auto SavePng(const Image &image, const std::string &path)
    -> std::expected<void, std::string>;

/// Reads a PNG and converts it to 8-bit RGBA.
auto LoadPng(const std::string &path) -> std::expected<Image, std::string>;

/// Result of comparing two images.
struct ImageDiff {
  /// False if the sizes differ; the counts are meaningless then.
  bool same_size = false;
  /// Pixels where any channel differs by more than the tolerance.
  std::int64_t differing_pixels = 0;
  /// Largest channel difference seen anywhere.
  int max_channel_diff = 0;
  /// Visualisation: differing pixels red, the rest a dimmed copy.
  Image diff;
};

/// Compares two images channel by channel.
/// @param[in] tolerance Largest channel difference that still counts as equal.
auto CompareImages(const Image &actual, const Image &expected, int tolerance)
    -> ImageDiff;

}  // namespace gloomy

#endif  // GLOOMY_CORE_IMAGE_H_
