#include "gloomy/ui/theme.h"

#include <gtest/gtest.h>

namespace gloomy::ui {
namespace {

TEST(ThemeTest, ScaledMultipliesMetricsAndKeepsColours) {
  const Theme base = Theme::Dark({});
  const Theme big = base.Scaled(1.5F);
  EXPECT_FLOAT_EQ(big.metrics.font_size, base.metrics.font_size * 1.5F);
  EXPECT_FLOAT_EQ(big.metrics.row_height, base.metrics.row_height * 1.5F);
  EXPECT_FLOAT_EQ(big.metrics.padding_x, base.metrics.padding_x * 1.5F);
  EXPECT_FLOAT_EQ(big.colors.text.r, base.colors.text.r);
  EXPECT_EQ(big.name, base.name);
}

TEST(ThemeTest, ScaledClampsAndKeepsLinesVisible) {
  const Theme base = Theme::Dark({});
  const Theme tiny = base.Scaled(0.01F);
  EXPECT_FLOAT_EQ(tiny.metrics.font_size,
                  base.metrics.font_size * Theme::kMinScale);
  EXPECT_GE(tiny.metrics.border_width, 1.0F);
  EXPECT_GE(tiny.metrics.focus_width, 1.0F);
  const Theme huge = base.Scaled(100.0F);
  EXPECT_FLOAT_EQ(huge.metrics.font_size,
                  base.metrics.font_size * Theme::kMaxScale);
  const Theme same = base.Scaled(1.0F);
  EXPECT_FLOAT_EQ(same.metrics.spacing, base.metrics.spacing);
}

}  // namespace
}  // namespace gloomy::ui
