#include "gloomy/ui/summary.h"

#include <gtest/gtest.h>

#include <vector>

#include "gloomy/ui/navigation.h"
#include "gloomy/ui/view_preset.h"
#include "gloomy/ui/widgets.h"
#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(SummaryTest, ExplainedLabelShowsWhyOnHover) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    ExplainedLabel(ctx, "##price", {"23.10 Cr", "Ore shortage at Mine 4"});
  };
  h.Frame(ui);
  const Vec2 c = h.Center(h.ctx().GetId("##price"));
  h.Frame(ui, {InputEvent::Move(c.x, c.y)});
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
  h.AdvanceTime(0.6);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
}

TEST(SummaryTest, ExplainedLabelTooltipOnKeyboardFocus) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    ExplainedLabel(ctx, "##price", {"23.10 Cr", "Ore shortage"});
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
}

TEST(SummaryTest, SummaryCardDrillsDown) {
  UiHarness h;
  const std::vector<float> spark{1, 3, 2, 5, 4};
  bool drill = false;
  auto ui = [&](Context &ctx) {
    SummarySpec spec;
    spec.title = "Ore price";
    spec.value = {"23.10 Cr", "30% under average"};
    spec.delta = "-4.2% today";
    spec.delta_tone = Tone::kGood;
    spec.sparkline = spark;
    drill = SummaryCard(ctx, "##ore", spec);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("##ore"))));
  EXPECT_TRUE(drill);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(drill) << "focused by the click";
}

TEST(SummaryTest, AnswerLineAction) {
  UiHarness h;
  bool act = false;
  auto ui = [&](Context &ctx) {
    act = AnswerLine(ctx,
                     "##best",
                     "Buy ore at Mine 4",
                     "30% under average",
                     "Show all trades");
  };
  h.Frame(ui);
  Id button = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##best"));
    button = ctx.GetId("Show all trades");
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(button)));
  EXPECT_TRUE(act);
}

// A map the app draws itself, navigable and searchable like a list.
class FakeMap : public NavigationTarget {
 public:
  std::vector<std::string> sectors{"Argon Prime",
                                   "Ore Belt",
                                   "Red Light",
                                   "Ore Mines",
                                   "Cloudbase"};
  std::int64_t cursor = -1;
  std::int64_t activated = -1;

  auto ItemCount() const -> std::int64_t override {
    return static_cast<std::int64_t>(sectors.size());
  }

  auto Cursor() const -> std::int64_t override { return cursor; }

  void SetCursor(std::int64_t i) override { cursor = i; }

  void Activate(std::int64_t i) override { activated = i; }

  auto ItemLabel(std::int64_t i) const -> std::string override {
    return sectors[static_cast<std::size_t>(i)];
  }
};

TEST(NavigationTest, SameKeysAsLists) {
  UiHarness h;
  FakeMap map;
  NavigationState nav;
  auto ui = [&](Context &ctx) {
    NodeDesc node;
    node.id = ctx.GetId("##map");
    node.flags = kNodeFocusable | kNodeInteractive;
    node.layout.width = SizeSpec::Fixed(200);
    node.layout.height = SizeSpec::Fixed(200);
    ctx.Open(node);
    ctx.Close();
    Navigate(ctx, ctx.GetId("##map"), map, nav);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  EXPECT_EQ(map.cursor, 0);
  h.Frame(ui, UiHarness::Key(Key::kEnd));
  EXPECT_EQ(map.cursor, 4);
  h.Frame(ui, {InputEvent::Text("o")});
  EXPECT_EQ(map.cursor, 1) << "type to jump wraps around";
  h.Frame(ui, {InputEvent::Text("o")});
  EXPECT_EQ(map.cursor, 3) << "the same letter again: next match";
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(map.activated, 3);
}

TEST(NavigationTest, MapIsSearchable) {
  FakeMap map;
  NavigationProvider provider("Map", map);
  provider.SetQuery("ore");
  EXPECT_EQ(provider.count(), 2U);
  std::vector<SearchHit> hits;
  provider.Collect(10, hits);
  ASSERT_EQ(hits.size(), 2U);
  EXPECT_EQ(hits[1].label, "Ore Mines");
  hits[1].go();
  EXPECT_EQ(map.cursor, 3);
}

TEST(SummaryPanelTest, ShowsTopRowsAndDrills) {
  UiHarness h({800, 500});
  auto table = std::make_shared<testing::MarketTable>(300);
  data::TableView view(table);
  data::Query q;
  q.sort = {{4, data::SortDirection::kAscending}};
  view.SetQuery(q);
  view.Update();
  const KeyNumber numbers[] = {{"Offers", {"300", ""}},
                               {"Cheapest", {"10.04 Cr", "Surplus"}}};
  const int columns[] = {0, 1, 4};
  bool drill = false;
  auto ui = [&](Context &ctx) {
    SummaryPanelSpec spec;
    spec.title = "Cheapest offers";
    spec.numbers = numbers;
    spec.columns = columns;
    spec.rows = 4;
    drill = SummaryPanel(ctx, "##panel", view, spec);
  };
  h.Frame(ui);
  EXPECT_LT(h.ctx().node_count(), 60);
  Id button = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##panel"));
    button = ctx.GetId("Open table");
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(button)));
  EXPECT_TRUE(drill);
}

TEST(ViewPresetTest, EncodeDecodeRoundTrip) {
  testing::MarketTable table(10);
  ViewPreset p;
  p.name = "Cheap ore; fresh";
  p.description = "Ore under 20 Cr, newest reports";
  p.query.sort = {{4, data::SortDirection::kAscending},
                  {6, data::SortDirection::kDescending}};
  p.query.filters = {
      {.column = 2, .op = data::FilterOp::kContains, .text = "Ore"},
      {.column = 4, .op = data::FilterOp::kLess, .a = 20},
      {.column = 3, .op = data::FilterOp::kInSet, .set = {0, 2}}};
  p.query.search = "\"ore belt\"";
  p.query.group_by = {1, 2};
  p.query.aggregates = {{4, data::AggregateOp::kMean},
                        {5, data::AggregateOp::kSum}};
  p.columns = {{.column = 0, .width = 110, .pinned = true},
               {.column = 4, .width = 90},
               {.column = 1, .width = 80, .visible = false}};
  const std::string text = EncodePreset(p, table);
  EXPECT_TRUE(text.starts_with("gloomy-view:1;"));
  EXPECT_EQ(text.find(' '), std::string::npos) << "safe to paste";
  const auto back = DecodePreset(text, table);
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(back->name, p.name);
  EXPECT_EQ(back->description, p.description);
  EXPECT_EQ(back->query, p.query);
  ASSERT_EQ(back->columns.size(), 3U);
  EXPECT_TRUE(back->columns[0].pinned);
  EXPECT_FALSE(back->columns[2].visible);
  EXPECT_FLOAT_EQ(back->columns[1].width, 90.0F);
}

TEST(ViewPresetTest, UnknownColumnsAreIgnored) {
  testing::MarketTable table(10);
  const auto p = DecodePreset(
      "gloomy-view:1;n=x;s=removed:a,price:d;c=removed:50:,station:70:", table);
  ASSERT_TRUE(p.has_value());
  ASSERT_EQ(p->query.sort.size(), 1U);
  EXPECT_EQ(p->query.sort[0].column, 4);
  ASSERT_EQ(p->columns.size(), 1U);
  EXPECT_FALSE(DecodePreset("not a preset", table).has_value());
}

TEST(ViewPresetTest, ApplyKeepsRenderersAndHidesUnlisted) {
  testing::MarketTable table(10);
  GridState state;
  state.columns = DefaultColumns(table);
  state.columns[5].renderer = CellRenderer::kBar;
  ViewPreset p;
  p.columns = {{.column = 5, .width = 60},
               {.column = 0, .width = 99, .pinned = true}};
  ApplyPreset(p, state);
  ASSERT_EQ(state.columns.size(), 7U);
  EXPECT_EQ(state.columns[0].column, 0) << "pinned first";
  EXPECT_EQ(state.columns[1].column, 5);
  EXPECT_EQ(state.columns[1].renderer, CellRenderer::kBar);
  EXPECT_FLOAT_EQ(state.columns[1].width, 60.0F);
  EXPECT_FALSE(state.columns[2].visible);
}

TEST(ViewPresetTest, PresetBarSelects) {
  UiHarness h({800, 200});
  std::vector<ViewPreset> presets(3);
  presets[0].name = "All";
  presets[0].curated = true;
  presets[1].name = "Cheap ore";
  presets[2].name = "Mine";
  int active = 0;
  PresetBarResult r;
  auto ui = [&](Context &ctx) {
    r = PresetBar(ctx, "##presets", presets, active);
  };
  h.Frame(ui);
  Id chip = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##presets"));
    chip = ctx.GetId(std::uint64_t{1});
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(chip)));
  EXPECT_TRUE(r.changed);
  EXPECT_EQ(active, 1);
}

}  // namespace
}  // namespace gloomy::ui
