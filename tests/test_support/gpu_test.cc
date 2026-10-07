#include "test_support/gpu_test.h"

#include <cstdlib>
#include <filesystem>

namespace gloomy::testing {

std::unique_ptr<gfx::DeviceContext> GpuTest::ctx_;

void GpuTest::SetUpTestSuite() {
  if (ctx_) {
    return;
  }
  gfx::DeviceOptions options;
  options.app_name = "gloomy-tests";
  options.validation = std::getenv("GLOOMY_NO_VALIDATION") == nullptr;
  auto ctx = gfx::DeviceContext::Create(options);
  ASSERT_TRUE(ctx.has_value()) << ctx.error();
  ctx_ = std::move(*ctx);
  if (!ctx_->validation_enabled()) {
    std::fprintf(stderr, "gloomy tests: running WITHOUT validation layers\n");
  }
}

void GpuTest::TearDownTestSuite() {
  // Destroyed here rather than at static destruction, when the Vulkan
  // loader and layers may already be gone.
  ctx_.reset();
}

void GpuTest::SetUp() {
  ASSERT_TRUE(ctx_ != nullptr);
  messages_before_ = ctx_->validation_message_count();
}

void GpuTest::TearDown() {
  if (ctx_) {
    EXPECT_EQ(ctx_->validation_message_count(), messages_before_)
        << "Vulkan validation reported problems";
  }
}

void ExpectMatchesReference(const Image &actual, const std::string &name,
                            int tolerance, int max_pixels) {
  namespace fs = std::filesystem;
  const fs::path reference = fs::path(GL_TEST_REFERENCE_DIR) / (name + ".png");
  const fs::path out_dir = GL_TEST_OUTPUT_DIR;
  fs::create_directories(out_dir);
  const fs::path actual_path = out_dir / (name + "_actual.png");
  ASSERT_TRUE(SavePng(actual, actual_path.string()).has_value());

  const char *update = std::getenv("GLOOMY_UPDATE_REFERENCES");
  if (update != nullptr && std::string(update) == "1") {
    ASSERT_TRUE(SavePng(actual, reference.string()).has_value());
    return;
  }
  auto expected = LoadPng(reference.string());
  ASSERT_TRUE(expected.has_value())
      << expected.error() << "\nActual image: " << actual_path
      << "\nRun with GLOOMY_UPDATE_REFERENCES=1 to create the reference.";
  const ImageDiff diff = CompareImages(actual, *expected, tolerance);
  ASSERT_TRUE(diff.same_size) << name << ": size " << actual.width << "x"
                              << actual.height << " differs from the reference "
                              << expected->width << "x" << expected->height;
  if (diff.differing_pixels > max_pixels) {
    const fs::path diff_path = out_dir / (name + "_diff.png");
    (void)SavePng(diff.diff, diff_path.string());
    ADD_FAILURE() << name << ": " << diff.differing_pixels
                  << " pixels differ (max channel difference "
                  << diff.max_channel_diff << ")\nActual: " << actual_path
                  << "\nDiff: " << diff_path;
  }
}

}  // namespace gloomy::testing
