#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/disclosure.h"
#include "gloomy/ui/overlays.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

struct Scene {
  int view = 0;
  TabMemory tabs;
};

// A detail pane in the progressive-disclosure style: views as segments,
// a card with the headline, key facts, the primary action and a More
// menu, tabs below with a section.
void Pane(Context &ctx, Scene &s) {
  static const std::vector<Segment> kViews = {
      {"Starter jobs", 3, ""}, {"Worth it", 2, ""}, {"Bounties", 0, ""}};
  static const std::vector<std::string> kTabs = {
      "Overview", "Terms", "Timeline", "Clauses"};
  static const std::vector<KeyNumber> kFacts = {
      {"Pays", {"+2,400 Cr", ""}, Tone::kGood},
      {"Capital", {"none", ""}, Tone::kNeutral},
      {"Time left", {"5h 40m", ""}, Tone::kWarning},
  };
  LayoutSpec root;
  root.padding = Padding::All(12);
  root.gap = 10;
  root.width = SizeSpec::Grow();
  root.height = SizeSpec::Grow();
  root.align = Align::kStretch;
  BeginColumn(ctx, root);
  SegmentedControl(ctx, "##views", s.view, kViews);
  BeginCard(ctx, "##card", {.status = Tone::kGood});
  LayoutSpec head;
  head.gap = 8;
  head.align = Align::kCenter;
  BeginRow(ctx, head);
  Heading(ctx, "Deliver 400 Food to Lunon Habitat");
  Chip(ctx, "Worth it", Tone::kGood);
  End(ctx);
  KeyFacts(ctx, "##facts", kFacts);
  BeginRow(ctx, head);
  BigButtonOptions big;
  big.caption = "Standard beacon, free";
  BigButton(ctx, "Bid 42/unit", big);
  if (BeginMenuButton(ctx, "More##more")) {
    MenuHeading(ctx, "Negotiate");
    MenuItem(ctx, MenuItemSpec{"Send an offer...", {}, "Above the maximum"});
    MenuItem(ctx, MenuItemSpec{"Subcontract...", {}, "Win it first", false});
    EndMenuButton(ctx);
  }
  End(ctx);
  EndCard(ctx);
  RememberedTabs(ctx, "##tabs", s.tabs, "supply", kTabs);
  SectionHeader(ctx, "Why it is worth it", "from your reports");
  EmptyState(ctx, "##empty", {"Nothing else", "Pick a contract", ""});
  End(ctx);
}

class DisclosureImageTest : public testing::GpuTest {
 protected:
  auto Render(testing::UiHarness &h) -> Image {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
    gfx::OffscreenTarget target(ctx(), {640, 400});
    return gfx::RenderToImage(ctx(),
                              renderer,
                              target,
                              h.ctx().draw_lists(),
                              h.ctx().theme().colors.background);
  }
};

TEST_F(DisclosureImageTest, CardTabsAndOpenMenu) {
  testing::UiHarness h({640, 400});
  Scene s;
  auto ui = [&](Context &ctx) {
    Pane(ctx, s);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.ctx().SetFocus(h.ctx().GetId("More##more"));
  h.Frame(ui, testing::UiHarness::Key(Key::kEnter));
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h), "disclosure_dark");
}

}  // namespace
}  // namespace gloomy::ui
