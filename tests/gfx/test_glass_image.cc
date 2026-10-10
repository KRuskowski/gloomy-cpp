#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

#include "gloomy/gfx/backdrop_blur.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/scene_renderer.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/action_bar.h"
#include "gloomy/ui/feed.h"
#include "gloomy/ui/list_rows.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::gfx {
namespace {

constexpr VkExtent2D kSize{480, 300};

void Barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from,
             VkImageLayout to, VkPipelineStageFlags2 src_stage,
             VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
             VkAccessFlags2 dst_access) {
  VkImageMemoryBarrier2 b{};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  b.srcStageMask = src_stage;
  b.srcAccessMask = src_access;
  b.dstStageMask = dst_stage;
  b.dstAccessMask = dst_access;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(cmd, &dep);
}

enum class Pattern : std::uint8_t {
  /// Saturated checkers and white stripes: the worst case for legibility.
  kBusy,
  /// Nearly white: glass must darken it enough for light text.
  kBright,
};

auto MakePattern(Pattern pattern) -> Image {
  Image img = Image::Make(static_cast<int>(kSize.width),
                          static_cast<int>(kSize.height));
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      std::uint8_t *p = img.At(x, y);
      if (pattern == Pattern::kBright) {
        const auto v = static_cast<std::uint8_t>(235 + (x * 20) / img.width);
        p[0] = v;
        p[1] = v;
        p[2] = static_cast<std::uint8_t>(v - 10);
      } else {
        const bool check = ((x / 24) + (y / 24)) % 2 == 0;
        const bool stripe = ((x + y) / 10) % 6 == 0;
        p[0] = stripe ? 255 : (check ? 230 : 20);
        p[1] = stripe ? 255 : (check ? 60 : 180);
        p[2] = stripe ? 255 : (check ? 40 : 240);
      }
      p[3] = 255;
    }
  }
  return img;
}

/// A host scene that shows a fixed picture: copies it into the frame and
/// hands it to glass as the backdrop.
class PatternScene : public SceneRenderer {
 public:
  PatternScene(DeviceContext &ctx, const Image &picture)
      : image_(ctx.CreateImage(kSize, 1, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
    Buffer staging = ctx.CreateBuffer(
        picture.pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
    std::memcpy(staging.mapped(), picture.pixels.data(), picture.pixels.size());
    ctx.SubmitAndWait([&](VkCommandBuffer cmd) {
      Barrier(cmd,
              image_.handle(),
              VK_IMAGE_LAYOUT_UNDEFINED,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_PIPELINE_STAGE_2_NONE,
              VK_ACCESS_2_NONE,
              VK_PIPELINE_STAGE_2_COPY_BIT,
              VK_ACCESS_2_TRANSFER_WRITE_BIT);
      VkBufferImageCopy copy{};
      copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {kSize.width, kSize.height, 1};
      vkCmdCopyBufferToImage(cmd,
                             staging.handle(),
                             image_.handle(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1,
                             &copy);
      Barrier(cmd,
              image_.handle(),
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              VK_PIPELINE_STAGE_2_COPY_BIT,
              VK_ACCESS_2_TRANSFER_WRITE_BIT,
              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    });
  }

  auto Active() const -> bool override { return true; }

  auto Record(VkCommandBuffer cmd, std::uint32_t, const ColorTarget &target)
      -> Backdrop override {
    Barrier(cmd,
            image_.handle(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT);
    Barrier(cmd,
            target.image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = copy.srcSubresource;
    copy.extent = {kSize.width, kSize.height, 1};
    vkCmdCopyImage(cmd,
                   image_.handle(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   target.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copy);
    Barrier(cmd,
            target.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    Barrier(cmd,
            image_.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    return {image_.handle(), image_.view(), kSize};
  }

 private:
  Image2D image_;
};

/// A panel, a card and a dialog-like box of text on glass.
void GlassUi(ui::Context &ctx) {
  ui::LayoutSpec root;
  root.padding = ui::Padding::All(16);
  root.gap = 16;
  root.width = ui::SizeSpec::Grow();
  root.height = ui::SizeSpec::Grow();
  ui::BeginRow(ctx, root);
  ui::LayoutSpec left;
  left.width = ui::SizeSpec::Fixed(200);
  left.height = ui::SizeSpec::Grow();
  ui::BeginPanel(ctx, left);
  ui::Heading(ctx, "Argon Prime");
  ui::Label(ctx, "Solar Power Plant");
  ui::LabelOptions muted;
  muted.muted = true;
  ui::Label(ctx, "Energy Cells 1,240 Cr", muted);
  ui::Button(ctx, "Dock", ui::ButtonKind::kPrimary);
  ui::Button(ctx, "Trade");
  ui::End(ctx);
  ui::Spacer(ctx);
  ui::LayoutSpec right;
  right.width = ui::SizeSpec::Fixed(150);
  ui::BeginPanel(ctx, right);
  ui::Label(ctx, "Hull 100%");
  ui::Label(ctx, "Shields 64%");
  ui::End(ctx);
  ui::End(ctx);
}

/// Lists on glass: an action bar, a feed, cell rows (one selected) and a
/// queue row, in a panel over the scene.
void ListsUi(ui::Context &ctx) {
  static const std::vector<ui::CellColumn> kColumns = {
      {"Ship", 90, ui::CellAlign::kLeft, {}},
      {"Order", 0, ui::CellAlign::kLeft, {}},
      {"Cr/h", 50, ui::CellAlign::kRight, {}},
  };
  static const std::vector<ui::RowAction> kActions = {
      {"Show", {}, ui::ButtonKind::kFlat}};
  static const std::vector<std::string_view> kOrders = {"Orders"};
  static const std::vector<ui::BarAction> kBar = {
      {"Company (M)", "Company", 1, ui::ButtonKind::kNormal, {}, {}},
      {"What should I do? (A)", "Advice", 3, ui::ButtonKind::kPrimary, {}, {}},
  };
  ui::LayoutSpec root;
  root.padding = ui::Padding::All(12);
  root.width = ui::SizeSpec::Grow();
  root.height = ui::SizeSpec::Grow();
  root.gap = 2;
  root.align = ui::Align::kStretch;
  ui::BeginPanel(ctx, root);
  ui::ActionBar(ctx, "##bar", kBar);
  const std::vector<ui::Notification> feed = {
      {"Loses 1,314 Cr/h",
       ui::Priority::kUrgent,
       "Courier 2",
       "just now",
       kOrders,
       {},
       true},
      {"Idle for 3 h",
       ui::Priority::kHigh,
       "Freighter",
       "1 h ago",
       kOrders,
       {},
       false},
  };
  ui::FeedOptions options;
  options.age_width = 60;
  options.source_width = 80;
  ui::NotificationFeed(ctx, "##feed", feed, options);
  ui::CellHeader(ctx, "##head", kColumns, kActions);
  for (int i = 0; i < 2; ++i) {
    const std::vector<ui::Cell> cells = {
        {i == 0 ? "Courier 2" : "Freighter",
         ui::Tone::kNeutral,
         {},
         true,
         false,
         {}},
        {i == 0 ? "Auto trade" : "Route 2/3",
         ui::Tone::kNeutral,
         {},
         false,
         false,
         {}},
        {i == 0 ? "-1,314" : "+420",
         i == 0 ? ui::Tone::kBad : ui::Tone::kGood,
         {},
         true,
         false,
         {}},
    };
    ui::CellRowOptions o;
    o.selected = i == 1;
    o.status = i == 0 ? ui::Tone::kBad : ui::Tone::kNeutral;
    o.actions = kActions;
    ctx.PushId(static_cast<std::uint64_t>(i));
    ui::CellRow(ctx, "##row", kColumns, cells, o);
    ctx.PopId();
  }
  ui::QueueRowSpec q;
  q.index = 0;
  q.count = 2;
  q.text = "Dock at Farm";
  q.current = true;
  ui::QueueRow(ctx, "##queue", q);
  ui::End(ctx);
}

/// Relative luminance of an sRGB pixel (WCAG).
auto Luminance(const std::uint8_t *p) -> double {
  auto lin = [](std::uint8_t c) {
    const double v = c / 255.0;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * lin(p[0]) + 0.7152 * lin(p[1]) + 0.0722 * lin(p[2]);
}

/// Worst WCAG contrast of the theme's text over the pixels of `r`.
auto WorstContrast(const Image &img, Rect r, const Color &text) -> double {
  const std::uint32_t packed = text.Pack();
  const std::uint8_t tp[4] = {static_cast<std::uint8_t>(packed & 0xFF),
                              static_cast<std::uint8_t>(packed >> 8 & 0xFF),
                              static_cast<std::uint8_t>(packed >> 16 & 0xFF),
                              255};
  const double lt = Luminance(tp);
  double worst = 1e9;
  for (int y = static_cast<int>(r.y0); y < static_cast<int>(r.y1); ++y) {
    for (int x = static_cast<int>(r.x0); x < static_cast<int>(r.x1); ++x) {
      const double lb = Luminance(img.At(x, y));
      const double c = (std::max(lt, lb) + 0.05) / (std::min(lt, lb) + 0.05);
      worst = std::min(worst, c);
    }
  }
  return worst;
}

class GlassImageTest : public testing::GpuTest {
 protected:
  auto RenderOver(testing::UiHarness &h, Pattern pattern) -> Image {
    UiRenderer renderer(ctx(), OffscreenTarget::kFormat, h.atlas(), 1);
    OffscreenTarget target(ctx(), kSize);
    BackdropBlur blur(ctx(), 1);
    PatternScene scene(ctx(), MakePattern(pattern));
    return RenderSceneToImage(
        ctx(),
        renderer,
        blur,
        scene,
        target,
        h.ctx().draw_lists(),
        BackdropBlur::LevelsForRadius(h.ctx().theme().glass.blur_radius));
  }

  void Build(testing::UiHarness &h) {
    h.ctx().SetTheme(ui::Theme::Dark(h.fonts()).WithGlass());
    h.Frame(GlassUi);
    h.Frame(GlassUi);
  }
};

TEST(BackdropBlurTest, LevelsGrowWithTheRadius) {
  EXPECT_EQ(BackdropBlur::LevelsForRadius(0.0F), 1);
  EXPECT_EQ(BackdropBlur::LevelsForRadius(4.0F), 1);
  EXPECT_EQ(BackdropBlur::LevelsForRadius(8.0F), 2);
  EXPECT_EQ(BackdropBlur::LevelsForRadius(24.0F), 4);
  EXPECT_EQ(BackdropBlur::LevelsForRadius(1e6F), BackdropBlur::kMaxLevels);
}

TEST_F(GlassImageTest, GlassOverBusyScene) {
  testing::UiHarness h({480.0F, 300.0F});
  Build(h);
  const Image img = RenderOver(h, Pattern::kBusy);
  testing::ExpectMatchesReference(img, "glass_busy", 2, 40);
  // Between the panels the scene shows as it is.
  const std::uint8_t *p = img.At(300, 250);
  const Image pattern = MakePattern(Pattern::kBusy);
  EXPECT_EQ(std::memcmp(p, pattern.At(300, 250), 4), 0);
  // Inside a panel it is blurred: neighbours across a checker edge differ
  // far less than in the scene.
  const int a = img.At(40, 250)[0];
  const int b = img.At(56, 250)[0];
  EXPECT_LT(std::abs(a - b), 60);
}

TEST_F(GlassImageTest, TextStaysReadableOverABrightScene) {
  testing::UiHarness h({480.0F, 300.0F});
  Build(h);
  const Image img = RenderOver(h, Pattern::kBright);
  testing::ExpectMatchesReference(img, "glass_bright", 2, 40);
  // A strip of the left panel without text, below the buttons.
  const Rect strip{30, 260, 200, 276};
  EXPECT_GE(WorstContrast(img, strip, h.ctx().theme().colors.text), 4.5);
}

TEST_F(GlassImageTest, TextStaysReadableOverABusyScene) {
  testing::UiHarness h({480.0F, 300.0F});
  Build(h);
  const Image img = RenderOver(h, Pattern::kBusy);
  const Rect strip{30, 260, 200, 276};
  EXPECT_GE(WorstContrast(img, strip, h.ctx().theme().colors.text), 4.5);
}

TEST_F(GlassImageTest, ListsOnGlassLetItShowThrough) {
  testing::UiHarness h({480.0F, 300.0F});
  h.ctx().SetTheme(ui::Theme::Dark(h.fonts()).WithGlass());
  h.Frame(ListsUi);
  h.Frame(ListsUi);
  const Image img = RenderOver(h, Pattern::kBusy);
  testing::ExpectMatchesReference(img, "glass_lists", 2, 40);
  // A row that is neither hovered nor selected paints nothing over the
  // glass, and the text stays readable on it: the empty end of its grow
  // column.
  ui::Context &c = h.ctx();
  c.PushId(std::uint64_t{0});
  const Id row = c.GetId("##row");
  c.PushId(row);
  const Rect order = c.LastRect(c.GetId("##c1")).value();
  c.PopId();
  c.PopId();
  const Rect strip{order.x1 - 40, order.y0 + 2, order.x1 - 4, order.y1 - 2};
  EXPECT_GE(WorstContrast(img, strip, c.theme().colors.text), 4.5);
}

TEST_F(GlassImageTest, WithoutASceneGlassIsSolid) {
  testing::UiHarness h({480.0F, 300.0F});
  Build(h);
  UiRenderer renderer(ctx(), OffscreenTarget::kFormat, h.atlas(), 1);
  OffscreenTarget target(ctx(), kSize);
  const Image img = RenderToImage(ctx(),
                                  renderer,
                                  target,
                                  h.ctx().draw_lists(),
                                  h.ctx().theme().colors.background);
  // The panel shows the theme's opaque surface, as without glass.
  const std::uint32_t surface =
      ui::Theme::Dark(h.fonts()).colors.surface.Pack();
  const std::uint8_t *p = img.At(100, 268);
  EXPECT_EQ(p[0], surface & 0xFF);
  EXPECT_EQ(p[1], surface >> 8 & 0xFF);
  EXPECT_EQ(p[2], surface >> 16 & 0xFF);
}

TEST_F(GlassImageTest, SolidThemesAreUnchanged) {
  // Glass is opt-in: the plain theme paints no glass at all.
  testing::UiHarness h({480.0F, 300.0F});
  h.Frame(GlassUi);
  h.Frame(GlassUi);
  for (const gfx::DrawList *list : h.ctx().draw_lists()) {
    for (const Instance &inst : list->instances()) {
      EXPECT_NE(inst.kind & 0xFFU,
                static_cast<std::uint32_t>(PrimitiveKind::kGlass));
    }
  }
}

}  // namespace
}  // namespace gloomy::gfx
