// Scroll areas reach their end: whatever a scroll area holds (wrapped
// text, rows of buttons, nested panels with padding), scrolling all the
// way down shows its last line fully, at every width and UI scale.

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "gloomy/data/document.h"
#include "gloomy/ui/chart.h"
#include "gloomy/ui/document_view.h"
#include "gloomy/ui/overlays.h"
#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

/// Lines like a sector's details during a war: long enough to wrap at
/// narrow widths.
auto Lines() -> std::vector<std::string> {
  return {
      "20 stations known; newest report live",
      "Travel 0:00 from your ship; your ships here: 3",
      "Explored: 26% of the sector",
      "Dark on the sector map: never seen; dimmed: seen, out of sensor "
      "range now",
      "Solari Concord: no toll for you, customs 11%",
      "Water field: 100% full, mines at 100% (surveyed 4m ago)",
      "Ore field: 100% full, mines at 100% (surveyed 4m ago)",
      "Veyra Dominion declared war on Solari Concord over Heson Prime",
      "Battle of Heson Prime: Veyra Dominion beat Solari Concord, 1 "
      "warships lost",
      "Veyra Dominion blockades Heson Prime against Solari Concord ships",
      "Spatial disturbance at the rim of Heson Prime: a jump arrives in "
      "about 4 h",
  };
}

struct Case {
  float width;
  float height;
  float scale;
};

class ScrollReachTest : public ::testing::TestWithParam<Case> {};

/// A details card: a padded panel around a scroll area with a header,
/// wrapped lines, a padded sub-panel and a row of buttons at the end.
void DetailsCard(Context &ctx, const Case &c, bool *last_clicked) {
  LayoutSpec panel;
  panel.width = SizeSpec::Fixed(c.width);
  panel.height = SizeSpec::Fixed(c.height);
  panel.padding = Padding::All(8 * c.scale);
  panel.gap = 5;
  panel.align = Align::kStretch;
  BeginPanel(ctx, panel);
  LayoutSpec body;
  body.width = SizeSpec::Grow();
  body.height = SizeSpec::Grow();
  body.gap = 5 * c.scale;
  body.align = Align::kStretch;
  BeginScroll(ctx, "##body", body);
  LabelOptions bold;
  bold.bold = true;
  Label(ctx, "Heson Prime", bold);
  LabelOptions wrap;
  wrap.wrap = true;
  wrap.width = SizeSpec::Grow();
  for (const std::string &line : Lines()) {
    Label(ctx, line, wrap);
  }
  LayoutSpec sub;
  sub.padding = Padding::All(6 * c.scale);
  sub.align = Align::kStretch;
  BeginPanel(ctx, sub);
  Label(ctx, "A boxed note that also wraps when the panel is narrow", wrap);
  End(ctx);
  LayoutSpec row;
  row.gap = 6;
  BeginRow(ctx, row);
  Button(ctx, "Open sector map (Period)");
  *last_clicked = Button(ctx, "Fly there (Enter)");
  End(ctx);
  EndScroll(ctx);
  End(ctx);
}

TEST_P(ScrollReachTest, LastLineFullyVisibleAtTheEnd) {
  const Case c = GetParam();
  UiHarness h({c.width + 40, c.height + 40});
  h.ctx().SetTheme(h.ctx().theme().Scaled(c.scale));
  bool clicked = false;
  auto ui = [&](Context &ctx) {
    DetailsCard(ctx, c, &clicked);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &ctx = h.ctx();
  ctx.PushId("##body");
  const Id content = ctx.GetId("##content");
  const Id last = ctx.GetId("Fly there (Enter)");
  ctx.PopId();
  const Vec2 centre = h.Center(content);
  // Wheel towards the user until nothing moves.
  for (int i = 0; i < 6; ++i) {
    h.Frame(ui,
            {InputEvent::Move(centre.x, centre.y), InputEvent::Wheel(0, -50)});
  }
  h.Frame(ui);
  const Rect view = ctx.LastRect(content).value();
  const Rect button = ctx.LastRect(last).value();
  EXPECT_LE(button.y1, view.y1 + 0.01F)
      << "width " << c.width << " scale " << c.scale;
  EXPECT_GE(button.y0, view.y0);
  EXPECT_TRUE(ctx.Overflows().empty())
      << "first: " << ctx.Overflows().front().text;
  // And the button takes a click there.
  h.Frame(ui, UiHarness::Click(button.Center()));
  EXPECT_TRUE(clicked);
}

INSTANTIATE_TEST_SUITE_P(
    Sizes, ScrollReachTest,
    ::testing::Values(Case{480, 300, 1.0F}, Case{360, 240, 1.0F},
                      Case{260, 200, 1.0F}, Case{180, 160, 1.0F},
                      Case{480, 300, 1.25F}, Case{360, 240, 1.5F},
                      Case{600, 400, 2.0F}, Case{260, 300, 0.75F}),
    [](const ::testing::TestParamInfo<Case> &info) {
      return "w" + std::to_string(static_cast<int>(info.param.width)) +
             "_scale" +
             std::to_string(static_cast<int>(info.param.scale * 100));
    });

/// A panel like station details: lines, a price chart, more lines and a
/// button at the end.
struct ChartPanel {
  std::vector<double> xs{0, 1, 2, 3, 4, 5};
  std::vector<double> ys{100, 110, 105, 120, 115, 130};
  ChartState chart;
  bool scroll = true;

  void operator()(Context &ctx) {
    LayoutSpec layout;
    layout.width = SizeSpec::Fixed(400);
    layout.height = SizeSpec::Fixed(300);
    layout.gap = 4;
    layout.align = Align::kStretch;
    if (scroll) {
      BeginScroll(ctx, "##panel", layout);
    } else {
      BeginColumn(ctx, layout);
    }
    for (int i = 0; i < 6; ++i) {
      Label(ctx, "Line above " + std::to_string(i));
    }
    const ChartSeries series{.name = "Price", .x = xs, .y = ys};
    ChartOptions options;
    options.height = SizeSpec::Fixed(scroll ? 200.0F : 120.0F);
    LineChart(ctx, "##chart", std::span(&series, 1), chart, options);
    if (scroll) {
      for (int i = 0; i < 12; ++i) {
        Label(ctx, "Line below " + std::to_string(i));
      }
    }
    Button(ctx, "Last");
    if (scroll) {
      EndScroll(ctx);
    } else {
      End(ctx);
    }
  }
};

// The wheel turned at one place scrolls a panel to its end although a
// chart passes under the pointer on the way: charts inside a scrolled
// panel zoom only with Ctrl held.
TEST(ScrollReachTest, AChartUnderThePointerDoesNotStopThePanel) {
  UiHarness h;
  ChartPanel panel;
  auto ui = [&](Context &ctx) {
    panel(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &ctx = h.ctx();
  ctx.PushId("##panel");
  const Id content = ctx.GetId("##content");
  const Id last = ctx.GetId("Last");
  ctx.PopId();
  const Rect view = ctx.LastRect(content).value();
  // Low in the panel: the chart comes up under the pointer while scrolling.
  const Vec2 at{view.Center().x, view.y1 - 20};
  for (int i = 0; i < 40; ++i) {
    h.Frame(ui, {InputEvent::Move(at.x, at.y), InputEvent::Wheel(0, -1)});
  }
  h.Frame(ui);
  EXPECT_FALSE(panel.chart.zoomed) << "the plain wheel scrolled the panel";
  const Rect button = ctx.LastRect(last).value();
  EXPECT_LE(button.y1, ctx.LastRect(content).value().y1 + 0.01F);

  // Ctrl+wheel over the chart zooms it (back at the top, where it shows).
  h.Frame([&](Context &c) {
    ui(c);
    ScrollOffset(c, "##panel") = 0.0;
  });
  h.Frame(ui);
  ctx.PushId("##panel");
  // LineChart() pushes its id as a number.
  ctx.PushId(ctx.GetId("##chart"));
  const std::optional<Rect> plot_rect = ctx.LastRect(ctx.GetId("##plot"));
  ctx.PopId();
  ctx.PopId();
  ASSERT_TRUE(plot_rect.has_value());
  const Vec2 plot = plot_rect->Center();
  InputEvent zoom = InputEvent::Wheel(0, 1);
  zoom.mods = kModCtrl;
  h.Frame(ui, {InputEvent::Move(plot.x, plot.y), zoom});
  h.Frame(ui);
  EXPECT_TRUE(panel.chart.zoomed);
}

// A chart with no scrolled panel around it zooms with the plain wheel.
TEST(ScrollReachTest, AChartOutsideAScrolledPanelZoomsWithTheWheel) {
  UiHarness h;
  ChartPanel panel;
  panel.scroll = false;
  auto ui = [&](Context &ctx) {
    panel(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &ctx = h.ctx();
  // LineChart() pushes its id as a number.
  ctx.PushId(ctx.GetId("##chart"));
  const Vec2 plot = h.Center(ctx.GetId("##plot"));
  ctx.PopId();
  h.Frame(ui, {InputEvent::Move(plot.x, plot.y), InputEvent::Wheel(0, 1)});
  h.Frame(ui);
  EXPECT_TRUE(panel.chart.zoomed);
}

// Content too tall for a column that does not scroll is reported, with
// its text; the same content in a scroll area is not.
TEST(OverflowTest, ReportsWhatACutOffColumnHolds) {
  UiHarness h;
  bool scroll = false;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.width = SizeSpec::Fixed(200);
    layout.height = SizeSpec::Fixed(100);
    if (scroll) {
      BeginScroll(ctx, "##area", layout);
    } else {
      BeginPanel(ctx, layout);
    }
    for (int i = 0; i < 10; ++i) {
      Label(ctx, "Line " + std::to_string(i));
    }
    if (scroll) {
      EndScroll(ctx);
    } else {
      End(ctx);
    }
  };
  h.Frame(ui);
  const std::vector<Overflow> &cut = h.ctx().Overflows();
  ASSERT_FALSE(cut.empty());
  EXPECT_EQ(cut.back().text, "Line 9");
  scroll = true;
  h.Frame(ui);
  EXPECT_TRUE(h.ctx().Overflows().empty());
}

// Wrapped text in a box too small for its lines, and a word too long
// for its column, are reported.
TEST(OverflowTest, ReportsWrappedTextTallerThanItsBoxAndLongWords) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    LabelOptions wrap;
    wrap.wrap = true;
    wrap.width = SizeSpec::Fixed(60);
    LayoutSpec box;
    box.width = SizeSpec::Fixed(60);
    box.height = SizeSpec::Fixed(20);
    BeginColumn(ctx, box);
    Label(ctx, "several words that wrap onto many lines", wrap);
    End(ctx);
    LayoutSpec narrow;
    narrow.width = SizeSpec::Fixed(30);
    BeginColumn(ctx, narrow);
    Label(ctx, "Incomprehensibilities", wrap);
    End(ctx);
  };
  h.Frame(ui);
  std::vector<std::string> texts;
  for (const Overflow &o : h.ctx().Overflows()) {
    texts.push_back(o.text);
  }
  EXPECT_NE(std::ranges::find(texts, "several words that wrap onto many lines"),
            texts.end());
  EXPECT_NE(std::ranges::find(texts, "Incomprehensibilities"), texts.end());
}

// Everything that fits reports nothing.
TEST(OverflowTest, NothingForAFittingPanel) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.padding = Padding::All(8);
    layout.gap = 4;
    BeginPanel(ctx, layout);
    Label(ctx, "Heson Prime");
    LabelOptions wrap;
    wrap.wrap = true;
    Label(ctx, "A short note", wrap);
    Button(ctx, "Fly there");
    End(ctx);
  };
  h.Frame(ui);
  EXPECT_TRUE(h.ctx().Overflows().empty());
}

/// Paragraphs of wide capitals: their laid-out height is well over the
/// estimate DocumentView starts from.
auto WideDocument() -> data::Document {
  std::string md;
  for (int i = 0; i < 40; ++i) {
    md += "MWMW QWMW " + std::to_string(i);
    for (int k = 0; k < 12; ++k) {
      md += " WMWMWM MWWM";
    }
    md += "\n\n";
  }
  md += "The last line.\n";
  return data::Document::ParseMarkdown(md);
}

/// The document's end shows: the scroll offset is at the exact total and
/// the last block is laid out.
void ExpectAtTheEnd(const DocumentViewState &st, float viewport) {
  ASSERT_FALSE(st.exact.empty());
  EXPECT_EQ(st.exact.back(), 1) << "the last block is laid out";
  double total = 0.0;
  for (const float h : st.heights) {
    total += h;
  }
  EXPECT_NEAR(st.scroll + viewport, total, 0.5);
}

// End shows the end at once, although the blocks there are taller than
// estimated.
TEST(DocumentReachTest, EndShowsTheLastLine) {
  UiHarness h({600, 400});
  const data::Document doc = WideDocument();
  DocumentViewState st;
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    DocumentView(ctx, "##doc", doc, st);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("##doc"))));
  h.Frame(ui, UiHarness::Key(Key::kEnd));
  const float viewport = h.ctx().LastRect(h.ctx().GetId("##doc"))->Height();
  ExpectAtTheEnd(st, viewport);
}

// Inside a scrolled card the wheel scrolls the document to its very end
// before the card: the document's wheel range is the one after layout.
TEST(DocumentReachTest, WheelReachesTheEndInsideAScrolledPanel) {
  UiHarness h({600, 400});
  const data::Document doc = WideDocument();
  DocumentViewState st;
  auto ui = [&](Context &ctx) {
    LayoutSpec card;
    card.width = SizeSpec::Fixed(560);
    card.height = SizeSpec::Fixed(360);
    card.align = Align::kStretch;
    BeginScroll(ctx, "##card", card);
    Label(ctx, "Header");
    LayoutSpec view;
    view.height = SizeSpec::Fixed(300);
    DocumentView(ctx, "##doc", doc, st, view);
    Label(ctx, "Footer");
    EndScroll(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  // BeginScroll() scopes ids by its label.
  h.ctx().PushId("##card");
  const Id id = h.ctx().GetId("##doc");
  h.ctx().PopId();
  for (int i = 0; i < 400 && st.exact.back() == 0; ++i) {
    const Vec2 c = h.Center(id);
    h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, -1)});
  }
  // A few more turns settle it at the end.
  for (int i = 0; i < 20; ++i) {
    const Vec2 c = h.Center(id);
    h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, -1)});
  }
  h.Frame(ui);
  const float viewport = h.ctx().LastRect(id)->Height();
  ExpectAtTheEnd(st, viewport);
}

// A child wider than a scroll area does not push the area and its scroll
// bar out of the panel: the area keeps the panel's width, the child is
// cut there and reported.
TEST(ScrollReachTest, AWideChildKeepsTheBarInThePanel) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    LayoutSpec panel;
    panel.width = SizeSpec::Fixed(300);
    panel.height = SizeSpec::Fixed(200);
    panel.padding = Padding::All(8);
    panel.align = Align::kStretch;
    BeginPanel(ctx, panel);
    LayoutSpec body;
    body.width = SizeSpec::Grow();
    body.height = SizeSpec::Grow();
    body.align = Align::kStretch;
    BeginScroll(ctx, "##body", body);
    for (int i = 0; i < 20; ++i) {
      Label(ctx, "Line " + std::to_string(i));
    }
    LayoutSpec wide;
    wide.width = SizeSpec::Fixed(500);
    BeginRow(ctx, wide);
    Label(ctx, "A row of fixed columns");
    End(ctx);
    EndScroll(ctx);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &ctx = h.ctx();
  ctx.PushId("##body");
  const Rect bar = ctx.LastRect(ctx.GetId("##bar")).value();
  const Rect content = ctx.LastRect(ctx.GetId("##content")).value();
  ctx.PopId();
  EXPECT_LE(bar.x1, 300.0F - 8.0F + 0.01F) << "the bar stays in the panel";
  EXPECT_LE(content.x1, bar.x0 + 0.01F);
  ASSERT_EQ(ctx.Overflows().size(), 1U);
  EXPECT_EQ(ctx.Overflows().front().text, "A row of fixed columns");
}

// A dialog never runs off a small screen: it is narrowed and capped to
// the viewport, its scrolled body shrinks and reaches its last line.
TEST(ScrollReachTest, ADialogFitsTheScreenAndItsBodyScrolls) {
  UiHarness h({640, 360});
  bool open = true;
  auto ui = [&](Context &ctx) {
    if (BeginDialog(ctx, "##dialog", "Contracts", open, 900.0F)) {
      LayoutSpec body;
      body.width = SizeSpec::Grow();
      body.height = SizeSpec::Grow();
      body.align = Align::kStretch;
      BeginScroll(ctx, "##body", body);
      for (int i = 0; i < 40; ++i) {
        Label(ctx, "Clause " + std::to_string(i));
      }
      Button(ctx, "Sign");
      EndScroll(ctx);
      EndDialog(ctx, open);
    }
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &ctx = h.ctx();
  ctx.PushId(ctx.GetId("##dialog"));
  const Rect panel = ctx.LastRect(ctx.GetId("##panel")).value();
  ctx.PushId("##body");
  const Id content = ctx.GetId("##content");
  const Id sign = ctx.GetId("Sign");
  ctx.PopId();
  ctx.PopId();
  EXPECT_GE(panel.x0, 0.0F);
  EXPECT_LE(panel.x1, 640.0F);
  EXPECT_GE(panel.y0, 0.0F);
  EXPECT_LE(panel.y1, 360.0F);
  EXPECT_TRUE(ctx.Overflows().empty()) << ctx.Overflows().front().text;
  const Vec2 c = h.Center(content);
  for (int i = 0; i < 10; ++i) {
    h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, -10)});
  }
  h.Frame(ui);
  EXPECT_LE(ctx.LastRect(sign)->y1, ctx.LastRect(content)->y1 + 0.01F);
}

}  // namespace
}  // namespace gloomy::ui
