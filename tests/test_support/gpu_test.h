#ifndef GLOOMY_TESTS_TEST_SUPPORT_GPU_TEST_H_
#define GLOOMY_TESTS_TEST_SUPPORT_GPU_TEST_H_

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "gloomy/core/image.h"
#include "gloomy/gfx/device_context.h"

namespace gloomy::testing {

/// Fixture owning one Vulkan device per test binary, with validation
/// enabled when the layer is installed. Every test fails if validation
/// reported anything while it ran.
class GpuTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite();

  static void TearDownTestSuite();

  void SetUp() override;

  void TearDown() override;

  static auto ctx() -> gfx::DeviceContext & { return *ctx_; }

 private:
  static std::unique_ptr<gfx::DeviceContext> ctx_;
  int messages_before_ = 0;
};

/// Compares `actual` with tests/reference/<name>.png. A channel may differ
/// by `tolerance`; at most `max_pixels` pixels may exceed it. On failure the
/// actual and diff images are written to <build>/test_output. With
/// GLOOMY_UPDATE_REFERENCES=1 the reference is (re)written instead.
void ExpectMatchesReference(const Image &actual, const std::string &name,
                            int tolerance = 2, int max_pixels = 0);

}  // namespace gloomy::testing

#endif  // GLOOMY_TESTS_TEST_SUPPORT_GPU_TEST_H_
