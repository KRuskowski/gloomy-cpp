#include "gloomy/ui/document_view.h"

#include <gtest/gtest.h>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/contract.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

class DocumentViewTest : public ::testing::Test {
 protected:
  DocumentViewTest()
      : doc_(data::Document::ParseMarkdown(testing::ContractText(500))) {}

  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          LayoutSpec root;
          root.width = SizeSpec::Grow();
          root.height = SizeSpec::Grow();
          BeginColumn(ctx, root);
          result_ = DocumentView(ctx, "##contract", doc_, state_);
          End(ctx);
        },
        std::move(events));
  }

  UiHarness h_{{600, 400}};
  data::Document doc_;
  DocumentViewState state_;
  DocumentResult result_;
};

TEST_F(DocumentViewTest, OnlyVisibleBlocksAreLaidOut) {
  Frame();
  Frame();
  EXPECT_GT(doc_.size(), 500U);
  EXPECT_LT(state_.layouts.size(), 40U);
  EXPECT_GT(state_.layouts.size(), 3U);
}

TEST_F(DocumentViewTest, SearchFindsAndStepsThroughMatches) {
  Frame();
  state_.search = "\"clause 250:\"";
  Frame();
  Frame();
  ASSERT_EQ(state_.matches.size(), 1U);
  EXPECT_EQ(MatchStatus(state_), "1 of 1");
  // The match's block is in view.
  const std::uint32_t block = state_.matches[0].block;
  EXPECT_LE(TopBlock(state_), block);
  EXPECT_GT(TopBlock(state_) + 20, block);
  state_.search = "ore belt";
  Frame();
  ASSERT_GT(state_.matches.size(), 100U);
  const int first = state_.current;
  GoToMatch(state_, false);
  Frame();
  EXPECT_EQ(state_.current, first + 1);
  GoToMatch(state_, true);
  GoToMatch(state_, true);
  Frame();
  EXPECT_EQ(
      state_.current,
      first - 1 < 0 ? static_cast<int>(state_.matches.size()) - 1 : first - 1);
  state_.search = "nothing like this";
  Frame();
  EXPECT_EQ(MatchStatus(state_), "No matches");
}

TEST_F(DocumentViewTest, KeysScrollAndOutlineJumps) {
  Frame();
  h_.ctx().SetFocus(h_.ctx().GetId("##contract"));
  Frame(UiHarness::Key(Key::kEnd));
  Frame();
  EXPECT_GT(TopBlock(state_), 400U);
  Frame(UiHarness::Key(Key::kHome));
  EXPECT_EQ(state_.scroll, 0.0);
  const auto outline = doc_.Outline();
  ASSERT_EQ(outline.size(), 3U);
  ScrollToBlock(state_, outline[2].block);
  Frame();
  EXPECT_EQ(TopBlock(state_), outline[2].block);
}

TEST_F(DocumentViewTest, LinkClickReportsTarget) {
  Frame();
  Frame();
  // The link "Smelter 2" is in the second block; find it by clicking along
  // the line.
  const Rect r = h_.ctx().LastRect(h_.ctx().GetId("##contract")).value();
  bool found = false;
  for (float x = r.x0 + 20; x < r.x1 && !found; x += 6) {
    for (float y = r.y0 + 40; y < r.y0 + 100 && !found; y += 6) {
      Frame(UiHarness::Click({x, y}));
      if (result_.link) {
        EXPECT_EQ(*result_.link, "station:2");
        found = true;
      }
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(DocumentViewTest, DragSelectsAndCopies) {
  Frame();
  Frame();
  const Rect r = h_.ctx().LastRect(h_.ctx().GetId("##contract")).value();
  // From the start of the title down into the next paragraph.
  const Vec2 a{r.x0 + 17, r.y0 + 12};
  const Vec2 b{r.x0 + 200, r.y0 + 70};
  Frame({InputEvent::Move(a.x, a.y), InputEvent::Down(a.x, a.y)});
  Frame({InputEvent::Move(b.x, b.y)});
  Frame({InputEvent::Up(b.x, b.y)});
  const std::string text = SelectedText(doc_, state_);
  EXPECT_TRUE(text.starts_with("Supply Contract 42\n\nBetween")) << text;
  Frame(UiHarness::Key(Key::kC, kModCtrl));
  EXPECT_EQ(h_.ctx().clipboard().get(), text);
  Frame(UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(SelectedText(doc_, state_).empty());
  Frame(UiHarness::Key(Key::kA, kModCtrl));
  Frame(UiHarness::Key(Key::kC, kModCtrl));
  EXPECT_TRUE(h_.ctx().clipboard().get().ends_with("settled weekly."));
}

class DocumentImageTest : public testing::GpuTest {};

TEST_F(DocumentImageTest, ContractMatchesReference) {
  UiHarness h({560, 520});
  const data::Document doc =
      data::Document::ParseMarkdown(testing::ContractText(3));
  DocumentViewState state;
  state.search = "ore";
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    root.padding = Padding::All(8);
    BeginColumn(ctx, root);
    DocumentView(ctx, "##contract", doc, state);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  state.scroll = 0;
  GoToMatch(state, false);
  // A selection from the middle of the first paragraph into the list.
  state.sel_anchor = DocPosition{1, 8};
  state.sel_focus = DocPosition{3, 14};
  h.Frame(ui);
  h.Frame(ui);
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {560, 520});
  const Image image = gfx::RenderToImage(ctx(),
                                         renderer,
                                         target,
                                         h.ctx().draw_lists(),
                                         h.ctx().theme().colors.background);
  testing::ExpectMatchesReference(image, "document");
}

}  // namespace
}  // namespace gloomy::ui
