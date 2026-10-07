#include <gtest/gtest.h>

#include <memory>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/data_grid.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

class DataGridImageTest : public testing::GpuTest {
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

TEST_F(DataGridImageTest, MarketGrid) {
  testing::UiHarness h({720, 300});
  auto table = std::make_shared<testing::MarketTable>(2000);
  data::TableView view(table);
  GridState state;
  state.columns = DefaultColumns(*table);
  state.columns[0].pinned = true;
  state.columns[0].width = 110;
  state.columns[0].renderer = CellRenderer::kAge;
  state.columns[0].age_column = 6;
  state.columns[0].age_fresh = 60;
  state.columns[0].age_stale = 540;
  state.columns[1].width = 110;
  state.columns[2].width = 120;
  state.columns[5].renderer = CellRenderer::kBar;
  state.columns[5].bar_max = 100000;
  state.query.sort = {{2, data::SortDirection::kAscending},
                      {4, data::SortDirection::kDescending}};
  state.query.search = "ore";
  state.query.filters = {
      {.column = 3, .op = data::FilterOp::kInSet, .set = {0, 1}}};
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(8);
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    DataGrid(ctx, "##market", view, state);
    ctx.Close();
  };
  h.Frame(ui);
  h.Frame(ui);
  h.ctx().SetFocus(h.ctx().GetId("##market"));
  h.Frame(ui, testing::UiHarness::Key(Key::kDown));
  h.Frame(ui, testing::UiHarness::Key(Key::kDown, kModShift));
  h.Frame(ui, testing::UiHarness::Key(Key::kDown, kModShift));
  testing::ExpectMatchesReference(Render(h, {720, 300}), "data_grid");
}

}  // namespace
}  // namespace gloomy::ui
