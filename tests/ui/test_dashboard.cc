#include "gloomy/ui/dashboard.h"

#include <gtest/gtest.h>

#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(DashboardTest, TileColumnsFollowTheWidth) {
  EXPECT_EQ(TileColumns(1000, 300, 12, 4), 3);
  EXPECT_EQ(TileColumns(1300, 300, 12, 4), 4);
  EXPECT_EQ(TileColumns(5000, 300, 12, 4), 4) << "at most max_columns";
  EXPECT_EQ(TileColumns(200, 300, 12, 4), 1) << "at least one";
  EXPECT_EQ(TileColumns(612, 300, 12, 4), 2) << "exactly two with the gap";
  EXPECT_EQ(TileColumns(611, 300, 12, 4), 1);
  EXPECT_EQ(TileColumns(1000, 0, 12, 4), 1);
}

/// Four tiles in a grid; returns the results of the last frame.
struct Hub {
  std::vector<TileLine> ships = {
      {"Courier 1", "+420 Cr/h", Tone::kGood, "Auto trade"},
      {"Freighter 2", "Under attack", Tone::kBad, {}},
      {"Freighter 3", "Idle 3 h", Tone::kWarning, {}},
  };
  std::vector<TileResult> results = std::vector<TileResult>(4);
  /// Null: the theme's cards.
  const PanelSkin *skin = nullptr;

  void Draw(Context &ctx) {
    BeginTileGrid(ctx, "##hub", 300);
    for (int i = 0; i < 4; ++i) {
      TileSpec spec;
      spec.title = "Ships";
      spec.aside = "3 ships";
      spec.answer = {"+1,200 Cr/h; one under attack", "Earnings last hour"};
      spec.status = i == 1 ? Tone::kBad : Tone::kNeutral;
      spec.lines = ships;
      spec.max_lines = 2;
      spec.action = "Fleet (O)";
      spec.skin = skin;
      results[static_cast<std::size_t>(i)] =
          DashboardTile(ctx, "##tile" + std::to_string(i), spec);
    }
    EndTileGrid(ctx);
  }
};

TEST(DashboardTest, GridTakesAsManyColumnsAsFit) {
  UiHarness h({1000, 700});
  Hub hub;
  auto ui = [&](Context &ctx) {
    hub.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Context &c = h.ctx();
  const Rect a = c.LastRect(h.ctx().GetId("##tile0")).value();
  const Rect b = c.LastRect(h.ctx().GetId("##tile1")).value();
  const Rect d = c.LastRect(h.ctx().GetId("##tile3")).value();
  EXPECT_NEAR(a.Width(), b.Width(), 0.5F) << "equal columns";
  EXPECT_FLOAT_EQ(a.y0, b.y0) << "side by side";
  EXPECT_GT(d.y0, a.y1) << "three columns: the fourth wraps";
  EXPECT_FLOAT_EQ(d.x0, a.x0);
  EXPECT_GT(a.Width(), 300.0F);
}

TEST(DashboardTest, NarrowGridStacksTiles) {
  UiHarness h({500, 900});
  Hub hub;
  auto ui = [&](Context &ctx) {
    hub.Draw(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect a = h.ctx().LastRect(h.ctx().GetId("##tile0")).value();
  const Rect b = h.ctx().LastRect(h.ctx().GetId("##tile1")).value();
  EXPECT_GT(b.y0, a.y1 - 0.5F);
  EXPECT_FLOAT_EQ(a.x0, b.x0);
}

TEST(DashboardTest, LineClickNamesTheLine) {
  UiHarness h({1000, 700});
  Hub hub;
  auto ui = [&](Context &ctx) {
    hub.Draw(ctx);
  };
  h.Frame(ui);
  Id line = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##tile2"));
    line = ctx.GetId("##line1");
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(line)));
  EXPECT_EQ(hub.results[2].line, 1);
  EXPECT_FALSE(hub.results[2].action) << "a line is not the action";
  EXPECT_EQ(hub.results[0].line, -1);
}

TEST(DashboardTest, MoreLineButtonAndEnterAct) {
  UiHarness h({1000, 700});
  Hub hub;
  auto ui = [&](Context &ctx) {
    hub.Draw(ctx);
  };
  h.Frame(ui);
  Id more = kNoId;
  Id button = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##tile0"));
    // Two of three lines shown: the third is "+1 more".
    more = ctx.GetId("##line3");
    button = ctx.GetId("Fleet (O)");
    ctx.PopId();
  });
  ASSERT_TRUE(h.ctx().LastRect(more).has_value()) << "+1 more is drawn";
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  EXPECT_TRUE(hub.results[0].action);
  h.Frame(ui);
  EXPECT_FALSE(hub.results[0].action);
  h.Frame(ui, UiHarness::Click(h.Center(button)));
  EXPECT_TRUE(hub.results[0].action);
  // A click on the tile itself focuses it; then Enter acts.
  h.Frame(ui);
  const Rect tile = h.ctx().LastRect(h.ctx().GetId("##tile3")).value();
  h.Frame(ui, UiHarness::Click({tile.x1 - 20, tile.y0 + 8}));
  EXPECT_TRUE(hub.results[3].action);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(hub.results[3].action) << "focused by the click";
}

TEST(DashboardTest, SkinnedTileKeepsItsContentInsideTheFrameAndActs) {
  PanelSkin skin;
  skin.frame = 6.0F;
  for (const PanelSkin *s : {static_cast<const PanelSkin *>(nullptr),
                             static_cast<const PanelSkin *>(&skin)}) {
    UiHarness h({1000, 700});
    Hub hub;
    hub.skin = s;
    auto ui = [&](Context &ctx) {
      hub.Draw(ctx);
    };
    h.Frame(ui);
    Id line = kNoId;
    h.Frame([&](Context &ctx) {
      ui(ctx);
      ctx.PushId(ctx.GetId("##tile2"));
      line = ctx.GetId("##line1");
      ctx.PopId();
    });
    Context &c = h.ctx();
    const Rect tile = c.LastRect(c.GetId("##tile2")).value();
    const Rect row = c.LastRect(line).value();
    const float frame = s != nullptr ? s->frame : 0.0F;
    EXPECT_GE(row.x0 - tile.x0, 12.0F + frame);
    EXPECT_GE(tile.x1 - row.x1, 12.0F + frame - 0.5F);
    h.Frame(ui, UiHarness::Click(h.Center(line)));
    EXPECT_EQ(hub.results[2].line, 1);
  }
}

TEST(DashboardTest, EmptyTileSaysWhy) {
  UiHarness h({800, 600});
  auto ui = [&](Context &ctx) {
    BeginTileGrid(ctx, "##hub", 300);
    TileSpec spec;
    spec.title = "Stations";
    spec.answer = {"No stations yet", {}};
    spec.empty = "Shipyards build them for you.";
    DashboardTile(ctx, "##stations", spec);
    EndTileGrid(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect r = h.ctx().LastRect(h.ctx().GetId("##stations")).value();
  EXPECT_GT(r.Height(), 50.0F) << "title, answer and the empty note";
}

}  // namespace
}  // namespace gloomy::ui
