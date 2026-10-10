#include "gloomy/ui/schematic.h"

#include <gtest/gtest.h>

#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

auto Rooms() -> std::vector<SchematicRoom> {
  std::vector<SchematicRoom> r(5);
  r[0] = {.label = "Engines", .kind = RoomKind::kEngine, .cells = 1};
  r[1] = {.label = "Hold", .kind = RoomKind::kHold, .cells = 4, .deck = 1};
  r[2] = {.label = "Shields", .kind = RoomKind::kShield, .cells = 1};
  r[3] = {.label = "Crew", .kind = RoomKind::kCrew, .cells = 2};
  r[4] = {.label = "Bridge", .kind = RoomKind::kBridge, .cells = 1};
  return r;
}

auto Overlap(const Rect &a, const Rect &b) -> bool {
  return !a.Intersect(b).Empty();
}

TEST(SchematicTest, AutoDecks) {
  EXPECT_EQ(AutoDecks(0), 1);
  EXPECT_EQ(AutoDecks(3), 1);
  EXPECT_EQ(AutoDecks(4), 2);
  EXPECT_EQ(AutoDecks(8), 2);
  EXPECT_EQ(AutoDecks(9), 3);
}

TEST(SchematicTest, LayoutPutsTheBridgeInTheBowAndFillsDecks) {
  const std::vector<SchematicRoom> rooms = Rooms();
  const Rect area{0, 0, 500, 140};
  const SchematicLayout l = LayoutSchematic(rooms, area, 2);
  ASSERT_EQ(l.rooms.size(), rooms.size());
  // Stern left, bow right.
  EXPECT_LT(l.engines.x1, l.body.x1);
  EXPECT_GE(l.bow_inner.x0, l.body.x1 - 0.5F);
  EXPECT_GE(l.bow_outer.x0, l.bow_inner.x1 - 0.5F);
  EXPECT_LT(l.bow_outer.Height(), l.bow_inner.Height());
  // The bridge sits in the bow.
  EXPECT_GE(l.rooms[4].x0, l.body.x1 - 0.5F);
  // The hold keeps its deck (the lower one); the rest fill deck 0, which
  // was emptier.
  EXPECT_GT(l.rooms[1].y0, l.rooms[0].y0);
  EXPECT_FLOAT_EQ(l.rooms[0].y0, l.rooms[2].y0);
  EXPECT_FLOAT_EQ(l.rooms[0].y0, l.rooms[3].y0);
  // Rooms of a deck share its length by their cells, in order.
  EXPECT_LT(l.rooms[0].x0, l.rooms[2].x0);
  EXPECT_LT(l.rooms[2].x0, l.rooms[3].x0);
  EXPECT_NEAR(l.rooms[3].Width(), 2.0F * l.rooms[0].Width(), 1.0F);
  // Inside the hull, never on top of each other.
  for (std::size_t i = 0; i < l.rooms.size(); ++i) {
    EXPECT_FALSE(l.rooms[i].Empty()) << i;
    EXPECT_TRUE(area.Contains(l.rooms[i].Min()));
    for (std::size_t j = i + 1; j < l.rooms.size(); ++j) {
      EXPECT_FALSE(Overlap(l.rooms[i], l.rooms[j])) << i << " " << j;
    }
  }
  EXPECT_EQ(RoomAt(l, l.rooms[1].Center()), 1);
  EXPECT_EQ(RoomAt(l, l.rooms[4].Center()), 4);
  EXPECT_EQ(RoomAt(l, {1, 1}), -1);
}

TEST(SchematicTest, DecksAreClampedAndTinyAreasLayNothing) {
  std::vector<SchematicRoom> rooms = Rooms();
  rooms[1].deck = 7;
  const SchematicLayout l = LayoutSchematic(rooms, {0, 0, 400, 120}, 2);
  EXPECT_GT(l.rooms[1].y0, l.rooms[0].y0) << "deck 7 is the last deck";
  const SchematicLayout none = LayoutSchematic(rooms, {0, 0, 10, 10}, 2);
  for (const Rect &r : none.rooms) {
    EXPECT_TRUE(r.Empty());
  }
}

TEST(SchematicTest, HoverAndClickNameTheRoom) {
  UiHarness h({640, 400});
  const std::vector<SchematicRoom> rooms = Rooms();
  const std::vector<SystemGauge> gauges = {
      {"Shields", 4, 3, 1, Tone::kGood, "300/400", "Shield points"},
      {"Hull", 5, 5, 0, Tone::kGood, "100%", {}},
  };
  SchematicSpec spec;
  spec.name = "Lunar Wind";
  spec.subtitle = "Container Freighter";
  spec.rooms = rooms;
  spec.gauges = gauges;
  spec.shield = 0.75F;
  spec.decks = 2;
  SchematicResult result;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(520)});
    result = ShipSchematic(ctx, "##ship", spec);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##ship"));
  const Id hull = c.GetId("##hull");
  c.PopId();
  const Rect area = c.LastRect(hull).value();
  const SchematicLayout l = LayoutSchematic(rooms, area, 2);
  const Vec2 hold = l.rooms[1].Center();
  h.Frame(ui, {InputEvent::Move(hold.x, hold.y)});
  EXPECT_EQ(result.hovered, 1);
  EXPECT_EQ(result.clicked, -1);
  h.Frame(ui, UiHarness::Click(l.rooms[4].Center()));
  EXPECT_EQ(result.clicked, 4);
  h.Frame(ui, {InputEvent::Move(area.x0 + 1, area.y0 + 1)});
  EXPECT_EQ(result.hovered, -1);
}

TEST(SchematicTest, CompactDrawsLess) {
  UiHarness h({640, 400});
  const std::vector<SchematicRoom> rooms = Rooms();
  const std::vector<SystemGauge> gauges = {
      {"Shields", 4, 3, 1, Tone::kGood, "300/400", {}},
  };
  SchematicSpec spec;
  spec.name = "Lunar Wind";
  spec.rooms = rooms;
  spec.gauges = gauges;
  int full = 0;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(520)});
    ShipSchematic(ctx, "##ship", spec);
    End(ctx);
  };
  h.Frame(ui);
  full = h.ctx().node_count();
  spec.compact = true;
  h.Frame(ui);
  EXPECT_LT(h.ctx().node_count(), full);
}

}  // namespace
}  // namespace gloomy::ui
