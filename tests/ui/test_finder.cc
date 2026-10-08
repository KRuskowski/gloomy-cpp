#include "gloomy/ui/finder.h"

#include <gtest/gtest.h>

#include <memory>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/contract.h"
#include "test_support/gpu_test.h"
#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

class FinderTest : public ::testing::Test {
 protected:
  FinderTest()
      : table_(std::make_shared<testing::MarketTable>(1000)),
        doc_(data::Document::ParseMarkdown(testing::ContractText(20))),
        stations_("Stations", table_, 0, {1, 2},
                  [this](data::RowIndex r) { chosen_row_ = r; }),
        contract_("Contract", doc_, [this](const data::DocMatch &m) {
          chosen_block_ = m.block;
        }) {
    commands_.Add({"Toggle map", "M", [this] {
                     ran_ = "map";
                   }});
    commands_.Add({"Open trade menu", "T", [this] {
                     ran_ = "trade";
                   }});
    commands_.Add({"Settlement report", "", [this] {
                     ran_ = "report";
                   }});
  }

  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          Button(ctx, "Under the finder");
          SearchProvider *providers[] = {&commands_, &stations_, &contract_};
          Finder(ctx, "##finder", providers, state_);
        },
        std::move(events));
  }

  void Type(std::string_view text) {
    for (const char c : text) {
      Frame({InputEvent::Text(std::string(1, c))});
    }
  }

  UiHarness h_{{900, 600}};
  std::shared_ptr<testing::MarketTable> table_;
  data::Document doc_;
  CommandProvider commands_;
  TableProvider stations_;
  DocumentProvider contract_;
  FinderState state_;
  std::string ran_;
  std::optional<data::RowIndex> chosen_row_;
  std::optional<std::uint32_t> chosen_block_;
};

TEST_F(FinderTest, ClosedFinderDrawsNothing) {
  Frame();
  EXPECT_EQ(h_.ctx().draw_lists().size(), 1U);
}

TEST_F(FinderTest, PaletteRanksFuzzyMatchesAndRuns) {
  state_.open = true;
  Frame();
  Frame();
  EXPECT_EQ(h_.ctx().draw_lists().size(), 2U);
  Type("tm");
  EXPECT_EQ(commands_.count(), 3U) << "all contain t...m";
  Frame(UiHarness::Key(Key::kEnter));
  EXPECT_EQ(ran_, "map");
  EXPECT_FALSE(state_.open);
}

TEST_F(FinderTest, GlobalSearchAcrossTablesAndDocuments) {
  state_.open = true;
  Frame();
  Type("\"station 12\"");
  for (int i = 0; i < 5; ++i) {
    Frame();
  }
  EXPECT_EQ(stations_.count(), 11U);
  EXPECT_EQ(commands_.count(), 0U);
  Frame(UiHarness::Key(Key::kDown));
  Frame(UiHarness::Key(Key::kEnter));
  ASSERT_TRUE(chosen_row_.has_value());
  EXPECT_EQ(table_->GetText(0, *chosen_row_).substr(0, 10), "Station 12");
}

TEST_F(FinderTest, DocumentHitsJumpToTheBlock) {
  state_.open = true;
  Frame();
  Type("escrow");
  Frame();
  Frame(UiHarness::Key(Key::kEnter));
  // The contract text has no "escrow"; nothing is chosen and the finder
  // stays open.
  EXPECT_TRUE(state_.open);
  state_.query = "\"clause 7:\"";
  Frame();
  Frame();
  EXPECT_EQ(contract_.count(), 1U);
  Frame(UiHarness::Key(Key::kEnter));
  ASSERT_TRUE(chosen_block_.has_value());
  EXPECT_NE(doc_.blocks()[*chosen_block_].text.find("Clause 7:"),
            std::string::npos);
}

// An app replaces the document under a provider (a contract's clauses
// rebuilt every tick): hits come from the new one, never from blocks that
// are gone.
TEST(DocumentProviderTest, DocumentReplacedAfterTheQuery) {
  data::Document doc = data::Document::ParseMarkdown(testing::ContractText(10));
  DocumentProvider provider("Contract", doc, [](const data::DocMatch &) {});
  provider.SetQuery("station");
  ASSERT_GT(provider.count(), 0U);
  doc = data::Document::ParseMarkdown("# Short\n\nNo match here.\n");
  std::vector<SearchHit> hits;
  provider.Collect(10, hits);
  EXPECT_TRUE(hits.empty());
  doc = data::Document::ParseMarkdown("# Again\n\nA station.\n");
  hits.clear();
  provider.Collect(10, hits);
  ASSERT_EQ(hits.size(), 1U);
  EXPECT_NE(hits[0].label.find("station"), std::string::npos);
}

TEST_F(FinderTest, EscapeClearsThenCloses) {
  state_.open = true;
  Frame();
  Type("ore");
  Frame(UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(state_.open);
  EXPECT_TRUE(state_.query.empty());
  Frame(UiHarness::Key(Key::kEscape));
  EXPECT_FALSE(state_.open);
}

TEST_F(FinderTest, ModalBlocksTheUiBelow) {
  Frame();
  const Vec2 c = h_.Center(h_.ctx().GetId("Under the finder"));
  state_.open = true;
  Frame();
  Frame(UiHarness::Click(c));
  EXPECT_FALSE(state_.open) << "a click on the background closes it";
}

class FinderImageTest : public testing::GpuTest {};

TEST_F(FinderImageTest, GlobalSearchMatchesReference) {
  UiHarness h({760, 460});
  auto table = std::make_shared<testing::MarketTable>(500);
  const data::Document doc =
      data::Document::ParseMarkdown(testing::ContractText(10));
  CommandProvider commands;
  commands.Add({"Open station list", "Ctrl+1", [] {
                }});
  commands.Add({"Show sector map", "M", [] {
                }});
  TableProvider stations("Stations", table, 0, {1, 2}, [](data::RowIndex) {});
  DocumentProvider contract("Contract 42", doc, [](const data::DocMatch &) {});
  FinderState state;
  state.open = true;
  state.query = "st";
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(16);
    BeginColumn(ctx, root);
    Heading(ctx, "Behind the finder");
    End(ctx);
    SearchProvider *providers[] = {&commands, &stations, &contract};
    Finder(ctx, "##finder", providers, state);
  };
  for (int i = 0; i < 4; ++i) {
    h.Frame(ui);
  }
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {760, 460});
  const Image image = gfx::RenderToImage(ctx(),
                                         renderer,
                                         target,
                                         h.ctx().draw_lists(),
                                         h.ctx().theme().colors.background);
  testing::ExpectMatchesReference(image, "finder");
}

}  // namespace
}  // namespace gloomy::ui
