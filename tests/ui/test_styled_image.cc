#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/cargo_hold.h"
#include "gloomy/ui/dashboard.h"
#include "gloomy/ui/paper.h"
#include "gloomy/ui/schematic.h"
#include "gloomy/ui/skin.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

// Stylised widgets as a game shows them: a contract with its stamp and
// clauses open, a stack of war bonds, an insurance policy; a ship's
// cross-section with a damaged room and its hold.

const std::vector<SheetFigure> kFigures = {
    {"Pays", "+2,400 Cr", Tone::kGood, {}},
    {"Due", "day 3 08:00", Tone::kWarning, {}},
    {"Penalty", "1,200 Cr", Tone::kBad, {}},
};
const std::vector<Signatory> kParties = {
    {"Issuer", "Lunon Habitat", Color::Rgb(0x78B4FF), true, "day 2"},
    {"Contractor", "Karl's Hauling", Color::Rgb(0x3CDC3C), true, "day 2"},
};
const std::vector<std::string_view> kClauses = {
    "Deliver 400 Food to Lunon Habitat by day 3 08:00.",
    "Paid 42 Cr per unit from escrow on delivery.",
};

void Documents(Context &ctx) {
  static const std::vector<SheetFigure> kTerms = {
      {"Price", "900 Cr", Tone::kNeutral, {}},
      {"Matures", "day 30", Tone::kNeutral, {}},
      {"Yield", "+11%", Tone::kGood, {}},
  };
  static const std::vector<SheetFigure> kCover = {
      {"Cover", "120,000 Cr", Tone::kGood, {}},
      {"Premium", "1,800 Cr", Tone::kNeutral, {}},
      {"Excess", "5,000 Cr", Tone::kNeutral, {}},
  };
  static const std::vector<std::string_view> kExclusions = {
      "War zones you knew of when you flew in",
      "Contraband and seized cargo",
  };
  LayoutSpec root;
  root.padding = Padding::All(12);
  root.gap = 12;
  root.width = SizeSpec::Grow();
  root.height = SizeSpec::Grow();
  BeginRow(ctx, root);
  LayoutSpec left;
  left.width = SizeSpec::Grow();
  left.gap = 12;
  left.align = Align::kStretch;
  BeginColumn(ctx, left);
  ContractSheetSpec sheet;
  sheet.issuer = {"Lunon Habitat",
                  "Argon Federation - pays on time",
                  Color::Rgb(0x78B4FF),
                  icon::kNoIcon};
  sheet.kind = "Supply contract";
  sheet.title = "Deliver 400 Food to Lunon Habitat";
  sheet.reference = "No. 42 - posted day 2 10:00";
  sheet.figures = kFigures;
  sheet.stamp = {"Awarded", Tone::kGood};
  sheet.parties = kParties;
  sheet.clauses = kClauses;
  BeginContractSheet(ctx, "##contract", sheet);
  Button(ctx, "Deliver 400 Food", ButtonKind::kPrimary);
  EndContractSheet(ctx, sheet);
  End(ctx);
  LayoutSpec right;
  right.width = SizeSpec::Fixed(340);
  right.gap = 12;
  right.align = Align::kStretch;
  BeginColumn(ctx, right);
  CertificateSpec bond;
  bond.issuer = {"Argon Federation",
                 "at war with the Paranid Empire",
                 Color::Rgb(0x3C78DC),
                 icon::kNoIcon};
  bond.title = "War bond";
  bond.face = "1,000 Cr";
  bond.terms = kTerms;
  bond.risk = "If Argon loses: 500 Cr back";
  bond.serial = "Series A - No. 0012";
  bond.held = 3;
  bond.ornament_seed = 2;
  bond.stamp = {"Outstanding", Tone::kNeutral};
  Certificate(ctx, "##bond", bond);
  PolicySpec policy;
  policy.insurer = {"Teladi Mutual",
                    "Profit is protection",
                    Color::Rgb(0x2E9E6A),
                    icon::kNoIcon};
  policy.title = "Cargo and hull";
  policy.number = "Policy 7-0031 - day 2 to day 9";
  policy.insured = "Courier 'Lunar Wind' and its cargo";
  policy.figures = kCover;
  policy.exclusions = kExclusions;
  policy.stamp = {"In force", Tone::kGood};
  PolicySheet(ctx, "##policy", policy);
  End(ctx);
  End(ctx);
}

const std::vector<CellColumn> kManifest = {
    {"Good", 0, CellAlign::kLeft, {}},
    {"Qty", 60, CellAlign::kRight, {}},
    {"Cost", 70, CellAlign::kRight, {}},
    {"Best buyer", 160, CellAlign::kLeft, {}},
};

void Ship(Context &ctx) {
  static const std::vector<SchematicRoom> kRooms = {
      {.label = "Engines",
       .kind = RoomKind::kEngine,
       .cells = 1.2F,
       .level = 0.8F},
      {.label = "Hold",
       .kind = RoomKind::kHold,
       .cells = 4,
       .deck = 1,
       .fill = 0.6F,
       .color = Color::Rgb(0x60C060)},
      {.label = "Shields",
       .kind = RoomKind::kShield,
       .cells = 1,
       .level = 0.5F,
       .damage = 0.6F},
      {.label = "Weapons",
       .kind = RoomKind::kWeapon,
       .cells = 1,
       .level = 1.0F},
      {.label = "Crew", .kind = RoomKind::kCrew, .cells = 1.5F, .crew = 4},
      {.label = "Bridge", .kind = RoomKind::kBridge, .cells = 1, .crew = 1},
  };
  static const std::vector<SystemGauge> kGauges = {
      {"Shields", 6, 3, 2, Tone::kWarning, "180/400", {}, icon::kNoIcon},
      {"Hull", 6, 5, 0, Tone::kGood, "86%", {}, icon::kNoIcon},
      {"Weapons", 4, 4, 0, Tone::kGood, "12/rd", {}, icon::kNoIcon},
      {"Sensors", 4, 2, 0, Tone::kNeutral, "18 km", {}, icon::kNoIcon},
      {"Engines", 4, 3, 0, Tone::kGood, "290 m/s", {}, icon::kNoIcon},
  };
  static const std::vector<Cell> kFood = {
      {"Food", Tone::kNeutral, {}, false, false, {}},
      {"360", Tone::kNeutral, {}, false, false, {}},
      {"12 Cr", Tone::kNeutral, {}, false, false, {}},
      {"Lunon Habitat 15", Tone::kGood, {}, false, false, {}}};
  static const std::vector<Cell> kWater = {
      {"Water", Tone::kNeutral, {}, false, false, {}},
      {"140", Tone::kNeutral, {}, false, false, {}},
      {"8 Cr", Tone::kNeutral, {}, false, false, {}},
      {"no buyer known", Tone::kNeutral, {}, false, true, {}}};
  static const std::vector<CargoLot> kLots = {
      {"Food", 360.0, Color::Rgb(0x60C060), {}, kFood},
      {"Water", 140.0, Color::Rgb(0x4080E0), {}, kWater},
  };
  static const std::vector<HoldSection> kSections = {
      {"Container", 1000.0, kLots}};
  LayoutSpec root;
  root.padding = Padding::All(12);
  root.gap = 12;
  root.width = SizeSpec::Grow();
  root.align = Align::kStretch;
  BeginColumn(ctx, root);
  SchematicSpec spec;
  spec.name = "Lunar Wind";
  spec.subtitle = "Container Freighter";
  spec.rooms = kRooms;
  spec.gauges = kGauges;
  spec.shield = 0.45F;
  spec.hull = 0.86F;
  spec.crew = "Crew 5 - 140 Cr/h";
  spec.decks = 2;
  spec.height = 150;
  ShipSchematic(ctx, "##ship", spec);
  CargoHoldSpec hold;
  hold.sections = kSections;
  hold.columns = kManifest;
  CargoHold(ctx, "##hold", hold);
  End(ctx);
}

class StyledImageTest : public testing::GpuTest {
 protected:
  auto Render(testing::UiHarness &h, VkExtent2D size) -> Image {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
    gfx::OffscreenTarget target(ctx(), size);
    return gfx::RenderToImage(ctx(),
                              renderer,
                              target,
                              h.ctx().draw_lists(),
                              h.ctx().theme().colors.background);
  }
};

TEST_F(StyledImageTest, Documents) {
  testing::UiHarness h({760, 640});
  auto ui = [](Context &ctx) {
    Documents(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  // Open the contract's clauses.
  Context &c = h.ctx();
  const Id toggle =
      HashCombine(HashCombine(c.GetId("##contract"), 0x434CULL), 0x636CULL);
  h.Frame(ui, testing::UiHarness::Click(h.Center(toggle)));
  h.Frame(ui, {InputEvent::Move(1, 1)});
  testing::ExpectMatchesReference(Render(h, {760, 640}), "styled_documents");
}

/// Goods of the hold matrix.
const Color kFoodColor = Color::Rgb(0x60C060);
const Color kPartsColor = Color::Rgb(0xC08040);
const Color kMetalColor = Color::Rgb(0xA0A8B8);
const Color kOreColor = Color::Rgb(0xB06040);
const Color kIceColor = Color::Rgb(0x80D0F0);

/// One class's hold empty, half, full and with mixed goods (a unit split
/// between two goods, a partly filled last unit).
struct HoldCase {
  std::string_view name;
  std::string_view type;
  double capacity;
  std::vector<CargoLot> half;
  std::vector<CargoLot> full;
  std::vector<CargoLot> mixed;
};

auto HoldCases() -> std::vector<HoldCase> {
  return {
      {"Courier (120 m3)",
       "Container",
       120.0,
       {{"Food", 60.0, kFoodColor, {}, {}}},
       {{"Food", 120.0, kFoodColor, {}, {}}},
       {{"Food", 35.0, kFoodColor, {}, {}},
        {"Hull Parts", 22.0, kPartsColor, {}, {}},
        {"Refined Metal", 17.0, kMetalColor, {}, {}}}},
      {"Container Freighter (600 m3)",
       "Container",
       600.0,
       {{"Food", 300.0, kFoodColor, {}, {}}},
       {{"Food", 600.0, kFoodColor, {}, {}}},
       {{"Food", 210.0, kFoodColor, {}, {}},
        {"Hull Parts", 140.0, kPartsColor, {}, {}},
        {"Refined Metal", 95.0, kMetalColor, {}, {}}}},
      {"Bulk Freighter (3,000 m3)",
       "Bulk",
       3000.0,
       {{"Ore", 1500.0, kOreColor, {}, {}}},
       {{"Ore", 3000.0, kOreColor, {}, {}}},
       {{"Ore", 1240.0, kOreColor, {}, {}}, {"Ice", 830.0, kIceColor, {}, {}}}},
  };
}

/// Each class in four states, or (with `shared`) the three classes half
/// full on one fleet-wide unit.
void Holds(Context &ctx, double shared) {
  static const std::vector<HoldCase> kCases = HoldCases();
  LayoutSpec root;
  root.padding = Padding::All(12);
  root.gap = 10;
  root.width = SizeSpec::Grow();
  root.align = Align::kStretch;
  BeginColumn(ctx, root);
  ctx.PushId("##holds");
  for (std::size_t i = 0; i < kCases.size(); ++i) {
    const HoldCase &c = kCases[i];
    ctx.PushId(static_cast<std::uint64_t>(i) + 1);
    LabelOptions bold;
    bold.bold = true;
    Label(ctx, c.name, bold);
    const std::vector<HoldSection> sections =
        shared > 0.0 ? std::vector<HoldSection>{{c.type, c.capacity, c.mixed}}
                     : std::vector<HoldSection>{{"Empty", c.capacity, {}},
                                                {"Half", c.capacity, c.half},
                                                {"Full", c.capacity, c.full},
                                                {"Mixed", c.capacity, c.mixed}};
    CargoHoldSpec spec;
    spec.sections = sections;
    spec.unit_volume = shared;
    CargoHold(ctx, "##hold", spec);
    ctx.PopId();
  }
  ctx.PopId();
  End(ctx);
}

TEST_F(StyledImageTest, HoldsFromCourierToBulkFreighter) {
  testing::UiHarness h({560, 720});
  auto ui = [](Context &ctx) {
    Holds(ctx, 0.0);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h, {560, 720}), "styled_holds");
}

TEST_F(StyledImageTest, HoldsOnASharedUnit) {
  testing::UiHarness h({760, 400});
  auto ui = [](Context &ctx) {
    Holds(ctx, 50.0);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h, {760, 400}), "styled_holds_shared");
}

TEST_F(StyledImageTest, ShipAndHold) {
  testing::UiHarness h({640, 460});
  auto ui = [](Context &ctx) {
    Ship(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h, {640, 460}), "styled_ship");
}

// Hub tiles in the theme's cards and in two HUD flavours.
constexpr std::string_view kMilitarySkin =
    "primary = #9CC870\nsecondary = #6E8C5A\nown = #9CC870\n"
    "warning = #E8B040\nalert = #E85A3C\npanel_paper = #141A10\n"
    "panel_text = #D8E0C8\npanel_text_muted = #8C9878\nline = 2\n"
    "panel_bolt = 2.5\npanel_corner = 2";
constexpr std::string_view kRaiderSkin =
    "primary = #FF5A3C\nsecondary = #A04A3A\nown = #FFB040\n"
    "warning = #FFB040\nalert = #FF2E2E\npanel_paper = #1C0E0C\n"
    "panel_text = #F0D8D0\npanel_text_muted = #A08078\nline = 1.5\n"
    "panel_bolt = 0\npanel_corner = 0";

void SkinnedTiles(Context &ctx) {
  static const PanelSkin kMilitary =
      ParsePanelSkin(kMilitarySkin, PanelSkin::FromTheme(ctx.theme()));
  static const PanelSkin kRaider =
      ParsePanelSkin(kRaiderSkin, PanelSkin::FromTheme(ctx.theme()));
  static const std::vector<TileLine> kLines = {
      {"Courier 1", "+420 Cr/h", Tone::kGood, {}},
      {"Freighter 2", "Under attack", Tone::kBad, {}},
      {"Freighter 3", "docked", Tone::kNeutral, {}},
  };
  const PanelSkin *skins[] = {nullptr, &kMilitary, &kRaider};
  const char *titles[] = {"Theme", "Military", "Raider"};
  BeginTileGrid(ctx, "##tiles", 200.0F, 3, 12.0F);
  for (int i = 0; i < 3; ++i) {
    TileSpec spec;
    spec.title = titles[i];
    spec.aside = "3 ships";
    spec.answer = {"+1,200 Cr/h", {}};
    spec.status = i == 2 ? Tone::kBad : Tone::kNeutral;
    spec.lines = kLines;
    spec.action = "Fleet (O)";
    spec.skin = skins[i];
    (void)DashboardTile(ctx, "##tile" + std::to_string(i), spec);
  }
  EndTileGrid(ctx);
}

TEST_F(StyledImageTest, SkinnedTiles) {
  testing::UiHarness h({720, 220});
  auto ui = [](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Grow(), .padding = Padding::All(12)});
    SkinnedTiles(ctx);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h, {720, 220}), "styled_tiles");
}

}  // namespace
}  // namespace gloomy::ui
