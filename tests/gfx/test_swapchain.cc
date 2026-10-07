#include "gloomy/gfx/swapchain.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/ui_renderer.h"

namespace gloomy::gfx {
namespace {

// A presentable surface without a display (VK_EXT_headless_surface), so
// the windowed render loop runs in tests. Only with a pinned driver (ctest
// pins lavapipe): NVIDIA's driver crashes when asked whether it can
// present to a headless surface.
auto CreateHeadlessContext() -> std::unique_ptr<DeviceContext> {
  if (std::getenv("VK_ICD_FILENAMES") == nullptr) {
    return nullptr;
  }
  DeviceOptions options;
  options.app_name = "gloomy-swapchain-test";
  options.validation = std::getenv("GLOOMY_NO_VALIDATION") == nullptr;
  options.instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME,
                                 "VK_EXT_headless_surface"};
  options.create_surface = [](VkInstance instance) {
    auto create = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT"));
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (create == nullptr) {
      return surface;
    }
    VkHeadlessSurfaceCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    create(instance, &info, nullptr, &surface);
    return surface;
  };
  auto ctx = DeviceContext::Create(options);
  return ctx ? std::move(*ctx) : nullptr;
}

TEST(SwapchainTest, RendersPresentsAndResizes) {
  auto ctx = CreateHeadlessContext();
  if (!ctx) {
    GTEST_SKIP() << "needs VK_ICD_FILENAMES and VK_EXT_headless_surface";
  }
  Atlas atlas(256, 1);
  Swapchain swapchain(*ctx, {320, 200});
  UiRenderer renderer(*ctx, swapchain.format(), atlas,
                      Swapchain::kFramesInFlight);
  DrawList list;
  list.FillRect(Rect::FromXywh(10, 10, 100, 50), Color::Rgb(0x4C8DF6),
                CornerRadii::All(6));
  const std::array<const DrawList *, 1> lists{&list};
  int presented = 0;
  for (int frame = 0; frame < 24; ++frame) {
    // Resize half way through: the swapchain is recreated.
    const VkExtent2D extent =
        frame < 12 ? VkExtent2D{320, 200} : VkExtent2D{400, 300};
    auto f = swapchain.BeginFrame(extent);
    if (!f) {
      continue;
    }
    renderer.Prepare(f->cmd, f->slot, lists);
    swapchain.BeginRendering(Color::Rgb(0x1E1E2E));
    renderer.Draw(f->cmd, f->slot, f->extent);
    swapchain.EndFrame();
    ++presented;
  }
  ctx->WaitIdle();
  EXPECT_GE(presented, 20);
  EXPECT_EQ(ctx->validation_message_count(), 0)
      << (ctx->ValidationMessages().empty() ? std::string()
                                            : ctx->ValidationMessages()[0]);
}

TEST(SwapchainTest, ZeroSizeSkipsFrames) {
  auto ctx = CreateHeadlessContext();
  if (!ctx) {
    GTEST_SKIP() << "needs VK_ICD_FILENAMES and VK_EXT_headless_surface";
  }
  Swapchain swapchain(*ctx, {64, 64});
  EXPECT_FALSE(swapchain.BeginFrame({0, 0}).has_value());
  EXPECT_EQ(ctx->validation_message_count(), 0);
}

}  // namespace
}  // namespace gloomy::gfx
