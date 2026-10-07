#include "gloomy/core/image.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace gloomy {
namespace {

TEST(ImageTest, PngRoundTrip) {
  Image image = Image::Make(3, 2);
  image.At(1, 1)[0] = 200;
  image.At(1, 1)[3] = 255;
  image.At(2, 0)[2] = 17;
  const std::string path =
      (std::filesystem::temp_directory_path() / "gloomy_test_image.png")
          .string();
  ASSERT_TRUE(SavePng(image, path).has_value());
  auto loaded = LoadPng(path);
  ASSERT_TRUE(loaded.has_value()) << loaded.error();
  EXPECT_EQ(loaded->width, 3);
  EXPECT_EQ(loaded->height, 2);
  EXPECT_EQ(loaded->pixels, image.pixels);
  std::filesystem::remove(path);
}

TEST(ImageTest, CompareCountsPixelsBeyondTolerance) {
  Image a = Image::Make(2, 2);
  Image b = Image::Make(2, 2);
  b.At(0, 0)[0] = 2;
  b.At(1, 1)[1] = 10;
  const ImageDiff diff = CompareImages(a, b, 2);
  EXPECT_TRUE(diff.same_size);
  EXPECT_EQ(diff.differing_pixels, 1);
  EXPECT_EQ(diff.max_channel_diff, 10);
  EXPECT_EQ(diff.diff.At(1, 1)[0], 255);
}

TEST(ImageTest, CompareDetectsSizeMismatch) {
  EXPECT_FALSE(
      CompareImages(Image::Make(1, 2), Image::Make(2, 1), 0).same_size);
}

TEST(ImageTest, LoadMissingFileFails) {
  EXPECT_FALSE(LoadPng("/nonexistent/gloomy.png").has_value());
}

}  // namespace
}  // namespace gloomy
