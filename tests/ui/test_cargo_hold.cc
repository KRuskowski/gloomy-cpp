#include "gloomy/ui/cargo_hold.h"

#include <gtest/gtest.h>

#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

auto Lots() -> std::vector<CargoLot> {
  return {
      {"Food",
       250.0,
       "250 units",
       Color::Rgb(0x60C060),
       icon::kNoIcon,
       CargoForm::kCrate,
       "Bought at 12 Cr"},
      {"Water",
       125.0,
       "125 units",
       Color::Rgb(0x4080E0),
       icon::kNoIcon,
       CargoForm::kTank,
       {}},
  };
}

TEST(CargoHoldTest, FillPoursLotsInOrder) {
  const std::vector<CargoLot> lots = Lots();
  // 1000 m3 in 8 bays of 125: food takes two bays, water one.
  const std::vector<HoldBay> bays = FillHold(1000.0, lots, 8);
  ASSERT_EQ(bays.size(), 8U);
  EXPECT_EQ(bays[0].lot, 0);
  EXPECT_EQ(bays[1].lot, 0);
  EXPECT_EQ(bays[2].lot, 1);
  EXPECT_FLOAT_EQ(bays[2].fill, 1.0F);
  for (std::size_t i = 3; i < bays.size(); ++i) {
    EXPECT_EQ(bays[i].lot, -1);
    EXPECT_FLOAT_EQ(bays[i].fill, 0.0F);
  }
}

TEST(CargoHoldTest, PartialBaysAndOverflow) {
  std::vector<CargoLot> lots = Lots();
  lots[0].volume = 180.0;
  // 180 food: one full bay and 55 of the next, which water fills up (70
  // of 125): water takes most of it.
  std::vector<HoldBay> bays = FillHold(1000.0, lots, 8);
  EXPECT_EQ(bays[0].lot, 0);
  EXPECT_EQ(bays[1].lot, 1);
  EXPECT_FLOAT_EQ(bays[1].fill, 1.0F);
  EXPECT_EQ(bays[2].lot, 1);
  EXPECT_NEAR(bays[2].fill, 55.0F / 125.0F, 1e-5F);
  // More than fits is cut off.
  lots[0].volume = 5000.0;
  bays = FillHold(1000.0, lots, 8);
  for (const HoldBay &b : bays) {
    EXPECT_EQ(b.lot, 0);
    EXPECT_FLOAT_EQ(b.fill, 1.0F);
  }
  EXPECT_TRUE(FillHold(0.0, lots, 0).empty());
  EXPECT_EQ(FillHold(0.0, lots, 4)[0].lot, -1);
}

TEST(CargoHoldTest, GridAndShare) {
  const HoldGrid g = HoldGridFor(24, 300.0F, 22.0F);
  EXPECT_EQ(g.columns, 11);
  EXPECT_EQ(g.rows, 3);
  EXPECT_EQ(HoldGridFor(4, 900.0F, 22.0F).columns, 4) << "no empty columns";
  EXPECT_EQ(AutoBays(300.0F, 22.0F), 22);
  EXPECT_EQ(AutoBays(60.0F, 22.0F), 8);
  EXPECT_DOUBLE_EQ(HoldUsed(1000.0, Lots()), 0.375);
  EXPECT_DOUBLE_EQ(HoldUsed(0.0, Lots()), 0.0);
}

TEST(CargoHoldTest, HoverOnABayOrTheLegendNamesTheLot) {
  UiHarness h({640, 400});
  const std::vector<CargoLot> lots = Lots();
  CargoHoldSpec spec;
  spec.capacity = 1000.0;
  spec.lots = lots;
  spec.bays = 8;
  spec.title = "Hold";
  CargoHoldResult result;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(400)});
    result = CargoHold(ctx, "##hold", spec);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##hold"));
  const Id grid = c.GetId("##grid");
  const Id water = c.GetId(std::uint64_t{2});
  c.PopId();
  const Rect r = c.LastRect(grid).value();
  // The third bay (water): 6 px padding, 22 px bays, 3 px gaps.
  const Vec2 third{r.x0 + 6 + 2 * 25 + 11, r.y0 + 6 + 11};
  h.Frame(ui, {InputEvent::Move(third.x, third.y)});
  EXPECT_EQ(result.hovered, 1);
  const Vec2 empty{r.x0 + 6 + 6 * 25 + 11, r.y0 + 6 + 11};
  h.Frame(ui, {InputEvent::Move(empty.x, empty.y)});
  EXPECT_EQ(result.hovered, -1);
  h.Frame(ui, UiHarness::Click(h.Center(water)));
  EXPECT_EQ(result.clicked, 1);
}

}  // namespace
}  // namespace gloomy::ui
