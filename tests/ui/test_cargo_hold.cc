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
      {"Food", 250.0, Color::Rgb(0x60C060), "Bought at 12 Cr", {}},
      {"Hull Parts", 125.0, Color::Rgb(0xC08040), {}, {}},
  };
}

TEST(CargoHoldTest, LadderOfUnitVolumes) {
  EXPECT_DOUBLE_EQ(LadderVolume(0), 10.0);
  EXPECT_DOUBLE_EQ(LadderVolume(1), 25.0);
  EXPECT_DOUBLE_EQ(LadderVolume(2), 50.0);
  EXPECT_DOUBLE_EQ(LadderVolume(3), 100.0);
  EXPECT_DOUBLE_EQ(LadderVolume(4), 250.0);
  EXPECT_DOUBLE_EQ(LadderVolume(6), 1000.0);
  // A courier, a container freighter, a bulk freighter, a giant: each
  // between 8 and 40 units.
  EXPECT_DOUBLE_EQ(UnitVolumeFor(120.0), 10.0);
  EXPECT_DOUBLE_EQ(UnitVolumeFor(600.0), 25.0);
  EXPECT_DOUBLE_EQ(UnitVolumeFor(3000.0), 100.0);
  EXPECT_DOUBLE_EQ(UnitVolumeFor(400000.0), 10000.0);
  for (const double c : {80.0, 120.0, 600.0, 3000.0, 20000.0, 400000.0}) {
    const double n = c / UnitVolumeFor(c);
    EXPECT_LE(n, 40.0) << c;
    EXPECT_GE(n, 8.0) << c;
  }
  EXPECT_EQ(UnitLegend(2500.0, "m3"), "1 unit = 2,500 m3");
}

TEST(CargoHoldTest, UnitsFillInOrderAndShareSplitUnits) {
  std::vector<CargoLot> lots = Lots();
  lots[0].volume = 60.0;
  lots[1].volume = 30.0;
  // 25 m3 units of a 200 m3 hold: food 60 = two units and 10 of the
  // third, which hull parts fill up (15), then 15 more in the fourth.
  const std::vector<HoldUnit> units = FillUnits(200.0, 25.0, lots);
  ASSERT_EQ(units.size(), 8U);
  ASSERT_EQ(units[0].pieces.size(), 1U);
  EXPECT_FLOAT_EQ(units[0].Fill(), 1.0F);
  ASSERT_EQ(units[2].pieces.size(), 2U);
  EXPECT_EQ(units[2].pieces[0].lot, 0);
  EXPECT_FLOAT_EQ(units[2].pieces[0].share, 0.4F);
  EXPECT_EQ(units[2].pieces[1].lot, 1);
  EXPECT_FLOAT_EQ(units[2].pieces[1].share, 0.6F);
  ASSERT_EQ(units[3].pieces.size(), 1U);
  EXPECT_FLOAT_EQ(units[3].Fill(), 0.6F);
  EXPECT_TRUE(units[4].pieces.empty());
}

TEST(CargoHoldTest, LastUnitIsSmallerAndOverflowIsCut) {
  std::vector<CargoLot> lots = Lots();
  lots[0].volume = 1000.0;
  // 130 m3 in 25 m3 units: five full, a sixth of 5 m3.
  const std::vector<HoldUnit> units = FillUnits(130.0, 25.0, lots);
  ASSERT_EQ(units.size(), 6U);
  EXPECT_FLOAT_EQ(units[5].size, 0.2F);
  EXPECT_FLOAT_EQ(units[5].Fill(), 0.2F);
  for (const HoldUnit &u : units) {
    ASSERT_EQ(u.pieces.size(), 1U) << "the second lot does not fit";
  }
  EXPECT_TRUE(FillUnits(0.0, 25.0, lots).empty());
  EXPECT_DOUBLE_EQ(HoldUsed(1000.0, Lots()), 0.375);
  EXPECT_DOUBLE_EQ(HoldUsed(100.0, Lots()), 1.0);
}

TEST(CargoHoldTest, HoverOnAUnitOrAManifestLineNamesTheLot) {
  UiHarness h({640, 400});
  const std::vector<CellColumn> columns = {{"Good", 0, CellAlign::kLeft, {}}};
  const std::vector<Cell> food = {
      {"Food", Tone::kNeutral, {}, false, false, {}}};
  const std::vector<Cell> parts = {
      {"Hull Parts", Tone::kNeutral, {}, false, false, {}}};
  std::vector<CargoLot> rows = Lots();
  rows[0].cells = food;
  rows[1].cells = parts;
  const std::vector<HoldSection> sections = {{"Container", 1000.0, rows}};
  CargoHoldSpec spec;
  spec.sections = sections;
  spec.columns = columns;
  CargoHoldResult result;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(500)});
    result = CargoHold(ctx, "##hold", spec);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##hold"));
  c.PushId(std::uint64_t{1});
  const Id area = c.GetId("##units");
  c.PopId();
  c.PushId(std::uint64_t{2});
  const Id parts_row = c.GetId("##lot");
  c.PopId();
  c.PopId();
  const Rect r = c.LastRect(area).value();
  // 1000 m3 in 25 m3 units: food 10 units, hull parts the next 5. Units
  // are 14 px with 3 px gaps after 5 px padding.
  const auto unit = [&r](int i) {
    return Vec2{r.x0 + 5 + static_cast<float>(i) * 17 + 7, r.y0 + 5 + 7};
  };
  h.Frame(ui, {InputEvent::Move(unit(2).x, unit(2).y)});
  EXPECT_EQ(result.section, 0);
  EXPECT_EQ(result.lot, 0);
  h.Frame(ui, {InputEvent::Move(unit(11).x, unit(11).y)});
  EXPECT_EQ(result.lot, 1);
  h.Frame(ui, {InputEvent::Move(unit(20).x, unit(20).y)});
  EXPECT_EQ(result.lot, -1) << "an empty unit";
  h.Frame(ui, UiHarness::Click(h.Center(parts_row)));
  EXPECT_EQ(result.clicked, 1);
}

TEST(CargoHoldTest, EmptyHoldSaysSo) {
  UiHarness h({640, 400});
  const std::vector<HoldSection> sections = {{"Bulk", 3000.0, {}}};
  const std::vector<CellColumn> columns = {{"Good", 0, CellAlign::kLeft, {}}};
  CargoHoldSpec spec;
  spec.sections = sections;
  spec.columns = columns;
  int with = 0;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(500)});
    CargoHold(ctx, "##hold", spec);
    End(ctx);
  };
  h.Frame(ui);
  with = h.ctx().node_count();
  spec.empty = {};
  h.Frame(ui);
  EXPECT_EQ(h.ctx().node_count(), with - 1);
}

}  // namespace
}  // namespace gloomy::ui
