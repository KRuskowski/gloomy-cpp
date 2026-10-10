#include "gloomy/ui/disclosure.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/ui/overlays.h"
#include "gloomy/ui/view_preset.h"
#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(DisclosureTest, CardHoldsItsHeadlineFactsAndChip) {
  UiHarness h({800, 600});
  const std::vector<KeyNumber> facts = {
      {"Capital", {"none", ""}, Tone::kGood},
      {"Time left", {"2h 05m", "Bidding closes day 1 14:00"}, Tone::kNeutral},
      {"Risk", {"-120 Cr", "Penalty 20% at your 75% record"}, Tone::kWarning},
  };
  Id fact = kNoId;
  auto ui = [&](Context &ctx) {
    BeginCard(ctx, "##card", {.status = Tone::kGood});
    Label(ctx, "Deliver 400 Food to Lunon Habitat");
    Chip(ctx, "Worth it", Tone::kGood);
    KeyFacts(ctx, "##facts", facts);
    EndCard(ctx);
    ctx.PushId("##facts");
    fact = ctx.GetId("##fact1");
    ctx.PopId();
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect card = h.ctx().LastRect(h.ctx().GetId("##card")).value();
  const Rect f = h.ctx().LastRect(fact).value();
  EXPECT_GT(card.Width(), 700.0F) << "a card grows to its parent's width";
  EXPECT_TRUE(card.Contains(f.Center()));
  EXPECT_GT(f.x0, card.x0 + 60.0F) << "facts sit side by side";
  // The fact's "why" shows on hover.
  h.Frame(ui, {InputEvent::Move(f.Center().x, f.Center().y)});
  h.AdvanceTime(0.6);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
}

TEST(DisclosureTest, SectionHeaderAndFormRowsLineUp) {
  UiHarness h({800, 600});
  std::string qty = "200";
  std::string price = "42";
  auto ui = [&](Context &ctx) {
    SectionHeader(ctx, "Terms", "as you know them");
    BeginFormRow(ctx, "Quantity", 100);
    TextField(ctx, "##qty", qty);
    EndFormRow(ctx);
    BeginFormRow(ctx, "Price per unit", 100);
    TextField(ctx, "##price", price);
    EndFormRow(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect a = h.ctx().LastRect(h.ctx().GetId("##qty")).value();
  const Rect b = h.ctx().LastRect(h.ctx().GetId("##price")).value();
  EXPECT_FLOAT_EQ(a.x0, b.x0) << "inputs line up after the label column";
  EXPECT_GT(a.x0, 100.0F);
  EXPECT_GT(b.y0, a.y1 - 1.0F);
}

TEST(DisclosureTest, BigButtonActivatesUnlessDisabled) {
  UiHarness h;
  bool disabled = false;
  bool pressed = false;
  auto ui = [&](Context &ctx) {
    BigButtonOptions opt;
    opt.caption = "Standard beacon, free";
    opt.disabled = disabled;
    opt.reason = "Dock at Lunon Habitat first";
    pressed = BigButton(ctx, "Bid 42/unit##primary", opt);
  };
  h.Frame(ui);
  const Id id = h.ctx().GetId("Bid 42/unit##primary");
  const Rect r = h.ctx().LastRect(id).value();
  EXPECT_GT(r.Height(), 36.0F) << "taller than a normal button";
  h.Frame(ui, UiHarness::Click(r.Center()));
  EXPECT_TRUE(pressed);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(pressed) << "Enter on the focused button";
  // Without emphasis it still acts, drawn in the surface colour.
  bool emphasis = true;
  auto plain = [&](Context &ctx) {
    BigButtonOptions opt;
    opt.emphasis = emphasis;
    pressed = BigButton(ctx, "Bid 42/unit##primary", opt);
  };
  emphasis = false;
  h.Frame(plain);
  h.Frame(plain, UiHarness::Click(r.Center()));
  EXPECT_TRUE(pressed);
  const std::uint32_t primary = h.ctx().theme().colors.primary.Pack();
  int primary_fills = 0;
  for (const gfx::DrawList *list : h.ctx().draw_lists()) {
    for (const gfx::Instance &i : list->instances()) {
      primary_fills += i.color == primary ? 1 : 0;
    }
  }
  EXPECT_EQ(primary_fills, 0);
  disabled = true;
  h.Frame(ui, UiHarness::Click(r.Center()));
  EXPECT_FALSE(pressed);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_FALSE(pressed);
}

TEST(DisclosureTest, EmptyStateOffersItsAction) {
  UiHarness h({800, 400});
  bool taken = false;
  auto ui = [&](Context &ctx) {
    taken = EmptyState(ctx,
                       "##empty",
                       {"No contracts known yet",
                        "Tenders reach your headquarters by beacon.",
                        "Ask the advisor"});
  };
  h.Frame(ui);
  h.ctx().PushId(h.ctx().GetId("##empty"));
  const Id action = h.ctx().GetId("Ask the advisor");
  h.ctx().PopId();
  const Rect box = h.ctx().LastRect(h.ctx().GetId("##empty")).value();
  const Rect r = h.ctx().LastRect(action).value();
  EXPECT_NEAR(r.Center().x, box.Center().x, 1.0F) << "centred";
  h.Frame(ui, UiHarness::Click(r.Center()));
  EXPECT_TRUE(taken);
}

TEST(DisclosureTest, SegmentedControlByMouseAndKeys) {
  UiHarness h({800, 200});
  int selected = 0;
  const std::vector<Segment> segments = {
      {"Starter jobs", 3, "Jobs that teach the game"},
      {"Worth it", 2, ""},
      {"Bounties", 0, ""},
  };
  bool changed = false;
  auto ui = [&](Context &ctx) {
    changed = SegmentedControl(ctx, "##views", selected, segments);
  };
  h.Frame(ui);
  h.ctx().PushId(h.ctx().GetId("##views"));
  const Id seg1 = h.ctx().GetId(std::uint64_t{1});
  const Id seg2 = h.ctx().GetId(std::uint64_t{2});
  h.ctx().PopId();
  h.Frame(ui, UiHarness::Click(h.Center(seg1)));
  EXPECT_TRUE(changed);
  EXPECT_EQ(selected, 1);
  h.Frame(ui, UiHarness::Key(Key::kRight));
  EXPECT_EQ(selected, 2);
  h.Frame(ui, UiHarness::Key(Key::kRight));
  EXPECT_EQ(selected, 0) << "wraps round";
  h.Frame(ui, UiHarness::Key(Key::kLeft));
  EXPECT_EQ(selected, 2);
  EXPECT_LT(h.ctx().LastRect(seg1)->x1, h.ctx().LastRect(seg2)->x0 + 0.5F);
}

TEST(DisclosureTest, TabMemoryKeepsTheLastTabPerGroup) {
  TabMemory memory;
  const std::vector<std::string> full = {
      "Overview", "Terms", "Timeline", "Negotiation", "Clauses"};
  const std::vector<std::string> short_set = {
      "Overview", "Terms", "Timeline", "Clauses"};
  EXPECT_EQ(memory.Get("supply", full), 0) << "the overview first";
  memory.Set("supply", "Negotiation");
  EXPECT_EQ(memory.Get("supply", full), 3);
  EXPECT_EQ(memory.Get("supply", short_set), 0)
      << "a tab that is not there falls back to the overview";
  EXPECT_EQ(memory.Get("freight", full), 0) << "groups are separate";
  memory.Set("supply", "Clauses");
  EXPECT_EQ(memory.Get("supply", short_set), 3);
  memory.Clear();
  EXPECT_EQ(memory.Get("supply", full), 0);
}

TEST(DisclosureTest, RememberedTabsStoreTheClickedTab) {
  UiHarness h({800, 200});
  TabMemory memory;
  const std::vector<std::string> tabs = {"Overview", "Terms", "Clauses"};
  std::string group = "supply";
  int shown = -1;
  auto ui = [&](Context &ctx) {
    shown = RememberedTabs(ctx, "##tabs", memory, group, tabs);
  };
  h.Frame(ui);
  EXPECT_EQ(shown, 0);
  h.ctx().PushId(h.ctx().GetId("##tabs"));
  const Id terms = h.ctx().GetId(std::uint64_t{1});
  h.ctx().PopId();
  h.Frame(ui, UiHarness::Click(h.Center(terms)));
  EXPECT_EQ(shown, 1);
  group = "freight";
  h.Frame(ui);
  EXPECT_EQ(shown, 0) << "another kind starts on its overview";
  group = "supply";
  h.Frame(ui);
  EXPECT_EQ(shown, 1) << "back to the kind: its last tab";
}

TEST(DisclosureTest, MenuButtonOpensChoosesAndCloses) {
  UiHarness h({800, 600});
  std::string chosen;
  auto ui = [&](Context &ctx) {
    Label(ctx, "Above");
    if (BeginMenuButton(ctx, "More##actions")) {
      MenuHeading(ctx, "Negotiate");
      if (MenuItem(ctx, MenuItemSpec{"Send an offer...", {}, "Above max"})) {
        chosen = "offer";
      }
      if (MenuItem(ctx,
                   MenuItemSpec{"Subcontract...", {}, "Win it first", false})) {
        chosen = "sub";
      }
      if (MenuItem(ctx, "Choose a channel...")) {
        chosen = "channel";
      }
      EndMenuButton(ctx);
    }
  };
  h.Frame(ui);
  const Id button = h.ctx().GetId("More##actions");
  const Rect b = h.ctx().LastRect(button).value();
  h.Frame(ui, UiHarness::Click(b.Center()));
  h.Frame(ui);
  ASSERT_EQ(h.ctx().draw_lists().size(), 2U) << "the menu opened";
  // Keyboard: the first item is highlighted; Down skips nothing, Enter
  // chooses.
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(chosen, "offer");
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U) << "closed after choosing";
  // The disabled item is never chosen.
  h.Frame(ui, UiHarness::Click(b.Center()));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(chosen, "offer");
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(chosen, "channel");
  // A second click on the button closes it again.
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(b.Center()));
  h.Frame(ui);
  ASSERT_EQ(h.ctx().draw_lists().size(), 2U);
  h.Frame(ui, UiHarness::Click(b.Center()));
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
  // Escape closes it too.
  h.Frame(ui, UiHarness::Click(b.Center()));
  h.Frame(ui);
  ASSERT_EQ(h.ctx().draw_lists().size(), 2U);
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U) << "Escape closes it";
}

TEST(DisclosureTest, MenuButtonAtTheRightEdgeKeepsItsWidth) {
  UiHarness h({800, 600});
  auto ui = [&](Context &ctx) {
    LayoutSpec row;
    row.width = SizeSpec::Grow();
    BeginRow(ctx, row);
    Spacer(ctx);
    if (BeginMenuButton(ctx, "New##new")) {
      MenuItem(ctx,
               MenuItemSpec{"Propose a supply deal...",
                            {},
                            "To the selected station's owner, at your price"});
      EndMenuButton(ctx);
    }
    End(ctx);
  };
  h.Frame(ui);
  const Id button = h.ctx().GetId("New##new");
  h.Frame(ui, UiHarness::Click(h.Center(button)));
  h.Frame(ui);
  h.Frame(ui);
  const Id popup = HashCombine(button, 0x6D656E75ULL);
  h.ctx().PushId(popup);
  const Id item = h.ctx().GetId(std::uint64_t{0});
  h.ctx().PopId();
  const Rect r = h.ctx().LastRect(item).value();
  EXPECT_GT(r.Width(), 200.0F) << "not squeezed into the space right of it";
  EXPECT_LE(r.x1, 800.0F);
}

TEST(DisclosureTest, DialogSummaryAndNestedDialogs) {
  UiHarness h({1000, 800});
  bool outer = true;
  bool inner = true;
  auto ui = [&](Context &ctx) {
    if (BeginDialog(ctx, "##contracts", "Contracts", outer, 800.0F)) {
      Label(ctx, "The list");
      DialogOptions opt;
      opt.width = 360.0F;
      opt.summary = "Offer 48/unit: 6 over the maximum";
      opt.tone = Tone::kWarning;
      opt.detail = "The issuer may accept, counter or reject";
      if (BeginDialog(ctx, "##offer", "Send an offer", inner, opt)) {
        Label(ctx, "Price");
        EndDialog(ctx, inner);
      }
      EndDialog(ctx, outer);
    }
  };
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 3U) << "base, outer, inner";
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_FALSE(inner) << "Escape closes the inner dialog first";
  EXPECT_TRUE(outer);
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_FALSE(outer);
}

TEST(DisclosureTest, PresetSegmentsMoveEmptyViewsToTheMenu) {
  UiHarness h({1000, 400});
  std::vector<ViewPreset> presets(4);
  presets[0].name = "Starter jobs";
  presets[1].name = "Bounties";
  presets[2].name = "War supply";
  presets[3].name = "All";
  const std::vector<int> counts = {3, 0, 0, 9};
  int active = 0;
  PresetBarResult result;
  auto ui = [&](Context &ctx) {
    PresetSegmentsOptions opt;
    opt.counts = counts;
    result = PresetSegments(ctx, "##views", presets, active, opt);
  };
  h.Frame(ui);
  h.ctx().PushId("##views");
  const Id segs = h.ctx().GetId("##segments");
  const Id more = h.ctx().GetId("Views##more");
  // Segments are declared under the control's id, inside the bar's.
  h.ctx().PushId(segs);
  const Id seg1 = h.ctx().GetId(std::uint64_t{1});
  const Id seg2 = h.ctx().GetId(std::uint64_t{2});
  h.ctx().PopId();
  h.ctx().PopId();
  EXPECT_TRUE(h.ctx().LastRect(seg1).has_value());
  EXPECT_FALSE(h.ctx().LastRect(seg2).has_value())
      << "two segments: Starter jobs and All; empty ones in the menu";
  // Segment 1 is "All".
  h.Frame(ui, UiHarness::Click(h.Center(seg1)));
  EXPECT_TRUE(result.changed);
  EXPECT_EQ(active, 3);
  // From the menu: the first hidden view (Bounties).
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.changed);
  EXPECT_EQ(active, 1);
  h.Frame(ui);
  EXPECT_TRUE(h.ctx().LastRect(seg2).has_value())
      << "the chosen view shows as a segment though empty";
  // Modified away from every preset: "Your view" is selected.
  active = -1;
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_EQ(active, -1);
}

TEST(DisclosureTest, PresetSegmentsSaveAndShareFromTheMenu) {
  UiHarness h({1000, 400});
  std::vector<ViewPreset> presets(1);
  presets[0].name = "All";
  int active = 0;
  PresetBarResult result;
  auto ui = [&](Context &ctx) {
    result = PresetSegments(ctx, "##views", presets, active);
  };
  h.Frame(ui);
  h.ctx().PushId("##views");
  const Id more = h.ctx().GetId("Views##more");
  h.ctx().PopId();
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.save);
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.share);
}

TEST(DisclosureTest, FitSegmentsDropsTheLastFirstAndKeepsTheChosen) {
  const std::vector<float> widths = {100, 100, 100, 100};
  const std::vector<bool> keep = {false, false, false, true};
  // Everything fits: 10 + 400 + 3 gaps of 2.
  EXPECT_EQ(FitSegments(widths, keep, 10, 2, 416),
            (std::vector<bool>{true, true, true, true}));
  // One must go: the last that may, the third.
  EXPECT_EQ(FitSegments(widths, keep, 10, 2, 400),
            (std::vector<bool>{true, true, false, true}));
  // Too narrow for even one: the kept one stays.
  EXPECT_EQ(FitSegments(widths, keep, 10, 2, 50),
            (std::vector<bool>{false, false, false, true}));
}

TEST(DisclosureTest, PresetSegmentsNeverCutANameAndCrowdIntoTheMenu) {
  UiHarness h({560, 400});
  std::vector<ViewPreset> presets(6);
  presets[0].name = "Starter jobs";
  presets[1].name = "Worth it";
  presets[2].name = "Freight for my ship";
  presets[3].name = "Tenders I can bid";
  presets[4].name = "My contracts";
  presets[5].name = "All";
  const std::vector<int> counts = {3, 3, 4, 10, 1, 11};
  int active = 0;
  PresetBarResult result;
  auto ui = [&](Context &ctx) {
    PresetSegmentsOptions opt;
    opt.counts = counts;
    result = PresetSegments(ctx, "##views", presets, active, opt);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.ctx().PushId("##views");
  const Id segs = h.ctx().GetId("##segments");
  const Id more = h.ctx().GetId("Views##more");
  h.ctx().PushId(segs);
  std::vector<Id> seg;
  for (std::uint64_t i = 0; i < 6; ++i) {
    seg.push_back(h.ctx().GetId(i));
  }
  h.ctx().PopId();
  h.ctx().PopId();
  const Id row = h.ctx().GetId("##views");
  ASSERT_TRUE(h.ctx().LastRect(row).has_value());
  ASSERT_TRUE(h.ctx().LastRect(more).has_value());
  EXPECT_LE(h.ctx().LastRect(more)->x1, h.ctx().LastRect(row)->x1)
      << "the menu button stays inside the row";
  // Each shown segment is as wide as its name and count need.
  int shown = 0;
  for (std::size_t i = 0; i < seg.size(); ++i) {
    if (const std::optional<Rect> r = h.ctx().LastRect(seg[i])) {
      ++shown;
      const ViewPreset &p = presets[i];
      EXPECT_GE(r->Width() + 0.5F,
                SegmentWidth(h.ctx(), Segment{p.name, counts[i], {}}, i == 0))
          << p.name << " is cut";
    }
  }
  EXPECT_GE(shown, 1);
  EXPECT_LT(shown, 6) << "560 px cannot hold six views";
  EXPECT_TRUE(h.ctx().LastRect(seg[0]).has_value()) << "the chosen stays";
  // The crowded views are in the menu, the first of them on top.
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.changed);
  EXPECT_EQ(active, shown) << "the first view that did not fit";
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_TRUE(
      h.ctx().LastRect(seg[0]).has_value() ||
      h.ctx().LastRect(seg[static_cast<std::size_t>(shown)]).has_value());
}

}  // namespace
}  // namespace gloomy::ui
