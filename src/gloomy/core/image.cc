#include "gloomy/core/image.h"

#include <png.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace gloomy {

auto Image::Make(int w, int h) -> Image {
  Image image;
  image.width = w;
  image.height = h;
  image.pixels.assign(static_cast<std::size_t>(w) * h * 4, 0);
  return image;
}

auto SavePng(const Image &image, const std::string &path)
    -> std::expected<void, std::string> {
  png_image png{};
  png.version = PNG_IMAGE_VERSION;
  png.width = static_cast<png_uint_32>(image.width);
  png.height = static_cast<png_uint_32>(image.height);
  png.format = PNG_FORMAT_RGBA;
  if (png_image_write_to_file(
          &png, path.c_str(), 0, image.pixels.data(), 0, nullptr) == 0) {
    std::string message = png.message;
    png_image_free(&png);
    return std::unexpected("cannot write " + path + ": " + message);
  }
  return {};
}

auto LoadPng(const std::string &path) -> std::expected<Image, std::string> {
  png_image png{};
  png.version = PNG_IMAGE_VERSION;
  if (png_image_begin_read_from_file(&png, path.c_str()) == 0) {
    return std::unexpected("cannot read " + path + ": " + png.message);
  }
  png.format = PNG_FORMAT_RGBA;
  Image image =
      Image::Make(static_cast<int>(png.width), static_cast<int>(png.height));
  if (png_image_finish_read(&png, nullptr, image.pixels.data(), 0, nullptr) ==
      0) {
    std::string message = png.message;
    png_image_free(&png);
    return std::unexpected("cannot decode " + path + ": " + message);
  }
  return image;
}

auto CompareImages(const Image &actual, const Image &expected, int tolerance)
    -> ImageDiff {
  ImageDiff result;
  if (actual.width != expected.width || actual.height != expected.height) {
    return result;
  }
  result.same_size = true;
  result.diff = Image::Make(actual.width, actual.height);
  for (int y = 0; y < actual.height; ++y) {
    for (int x = 0; x < actual.width; ++x) {
      const std::uint8_t *a = actual.At(x, y);
      const std::uint8_t *e = expected.At(x, y);
      int worst = 0;
      for (int c = 0; c < 4; ++c) {
        worst = std::max(worst, std::abs(int{a[c]} - int{e[c]}));
      }
      result.max_channel_diff = std::max(result.max_channel_diff, worst);
      std::uint8_t *d = result.diff.At(x, y);
      if (worst > tolerance) {
        ++result.differing_pixels;
        d[0] = 255;
        d[1] = 0;
        d[2] = 0;
      } else {
        for (int c = 0; c < 3; ++c) {
          d[c] = static_cast<std::uint8_t>(e[c] / 4);
        }
      }
      d[3] = 255;
    }
  }
  return result;
}

}  // namespace gloomy
