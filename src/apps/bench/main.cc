// gloomy_bench: frame times of a million-row grid while scrolling,
// searching and sorting, rendered offscreen.
//
//   gloomy_bench [--rows N] [--size WxH] [--validation]
//
// Use a release build. VK_ICD_FILENAMES picks lavapipe, GLOOMY_DEVICE a
// GPU. UI time is the CPU time of one frame (input, declaration, layout,
// painting into draw lists); GPU time is upload, draw and wait.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/data/table_view.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/data_grid.h"
#include "gloomy/ui/theme.h"
#include "gloomy/ui/widgets.h"
#include "sample_data.h"

namespace {

using Clock = std::chrono::steady_clock;
using gloomy::ui::InputEvent;
using gloomy::ui::Key;

auto Ms(Clock::duration d) -> double {
  return std::chrono::duration<double, std::milli>(d).count();
}

auto Average(const std::vector<double> &v) -> double {
  return v.empty() ? 0.0
                   : std::accumulate(v.begin(), v.end(), 0.0) /
                         static_cast<double>(v.size());
}

auto Percentile(std::vector<double> v, double p) -> double {
  if (v.empty()) {
    return 0.0;
  }
  std::sort(v.begin(), v.end());
  return v[static_cast<std::size_t>(p * static_cast<double>(v.size() - 1))];
}

// Frame time samples of one scenario.
struct Samples {
  std::vector<double> ui;
  std::vector<double> gpu;

  void Print(const char *name) const {
    std::printf("| %-28s | %5zu | %6.2f | %6.2f | %6.2f | %6.2f | %6.2f |\n",
                name,
                ui.size(),
                Average(ui),
                Percentile(ui, 0.95),
                Percentile(ui, 1.0),
                Average(gpu),
                Percentile(gpu, 0.95));
  }
};

// Query latency samples: time from the input until the view shows the
// result, and frames rendered meanwhile.
struct Latency {
  std::vector<double> ms;
  std::vector<int> frames;

  void Print(const char *name) const {
    const double avg_frames =
        frames.empty() ? 0.0
                       : std::accumulate(frames.begin(), frames.end(), 0.0) /
                             static_cast<double>(frames.size());
    std::printf("| %-28s | %5zu | %8.1f | %8.1f | %8.1f |\n",
                name,
                ms.size(),
                Average(ms),
                Percentile(ms, 1.0),
                avg_frames);
  }
};

// The benchmarked screen: a search field and the market grid.
class Bench {
 public:
  Bench(gloomy::gfx::DeviceContext &device, int rows, VkExtent2D size)
      : device_(device),
        ids_(fonts_.AddDefaultFonts()),
        text_(fonts_, atlas_),
        ctx_(text_, gloomy::ui::Theme::Dark(ids_)),
        market_(std::make_shared<gloomy::apps::SampleMarket>(rows)),
        view_(market_),
        renderer_(device, gloomy::gfx::OffscreenTarget::kFormat, atlas_),
        target_(device, size),
        size_(size) {
    grid_.columns = gloomy::ui::DefaultColumns(*market_);
    grid_.columns[gloomy::apps::kStation].pinned = true;
    grid_.columns[gloomy::apps::kStock].renderer =
        gloomy::ui::CellRenderer::kBar;
    grid_.columns[gloomy::apps::kStock].bar_max = 50000;
    view_.Finish();
  }

  // Runs one frame; returns UI and GPU milliseconds.
  auto Frame(std::vector<InputEvent> events = {}) -> std::pair<double, double> {
    time_ += 1.0 / 60.0;
    const auto start = Clock::now();
    ctx_.BeginFrame(
        {static_cast<float>(size_.width), static_cast<float>(size_.height)},
        events,
        time_);
    gloomy::ui::LayoutSpec root;
    root.padding = gloomy::ui::Padding::All(8);
    root.gap = 8;
    root.width = gloomy::ui::SizeSpec::Grow();
    root.height = gloomy::ui::SizeSpec::Grow();
    gloomy::ui::BeginColumn(ctx_, root);
    std::string status = std::to_string(view_.rows().size()) + " rows";
    gloomy::ui::SearchFieldOptions opt;
    opt.status = status;
    opt.busy = view_.busy();
    if (gloomy::ui::SearchField(ctx_, "##search", search_, opt).changed) {
      grid_.query.search = search_;
    }
    gloomy::ui::LayoutSpec grid_layout;
    grid_layout.height = gloomy::ui::SizeSpec::Grow();
    gloomy::ui::DataGrid(ctx_, "##grid", view_, grid_, grid_layout);
    gloomy::ui::End(ctx_);
    ctx_.EndFrame();
    const auto mid = Clock::now();
    device_.SubmitAndWait([&](VkCommandBuffer cmd) {
      renderer_.Prepare(cmd, 0, ctx_.draw_lists());
      target_.Begin(cmd, ctx_.theme().colors.background);
      renderer_.Draw(cmd, 0, size_);
      target_.End(cmd, false);
    });
    return {Ms(mid - start), Ms(Clock::now() - mid)};
  }

  void Focus(const char *label) { ctx_.SetFocus(ctx_.GetId(label)); }

  void FocusSearch() {
    ctx_.SetFocus(gloomy::ui::SearchFieldId(ctx_, "##search"));
  }

  auto view() -> gloomy::data::TableView & { return view_; }

  auto grid() -> gloomy::ui::GridState & { return grid_; }

  auto instances() const -> std::uint32_t {
    return renderer_.instance_count(0);
  }

 private:
  gloomy::gfx::DeviceContext &device_;
  gloomy::text::FontSet fonts_;
  gloomy::text::DefaultFonts ids_;
  gloomy::Atlas atlas_{1024, 4};
  gloomy::text::TextSystem text_;
  gloomy::ui::Context ctx_;
  std::shared_ptr<gloomy::apps::SampleMarket> market_;
  gloomy::data::TableView view_;
  gloomy::ui::GridState grid_;
  std::string search_;
  gloomy::gfx::UiRenderer renderer_;
  gloomy::gfx::OffscreenTarget target_;
  VkExtent2D size_;
  double time_ = 1.0;
};

// Runs frames until the view has applied the latest query.
auto WaitForView(Bench &bench, Samples &samples) -> int {
  int frames = 0;
  do {
    auto [ui, gpu] = bench.Frame();
    samples.ui.push_back(ui);
    samples.gpu.push_back(gpu);
    ++frames;
  } while (bench.view().busy() ||
           bench.view().view().query != bench.grid().query);
  return frames;
}

}  // namespace

auto main(int argc, char **argv) -> int {
  int rows = 1000000;
  VkExtent2D size{1280, 800};
  bool validation = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--rows") == 0 && i + 1 < argc) {
      rows = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
      unsigned w = 0;
      unsigned h = 0;
      if (std::sscanf(argv[++i], "%ux%u", &w, &h) == 2) {
        size = {w, h};
      }
    } else if (std::strcmp(argv[i], "--validation") == 0) {
      validation = true;
    }
  }
  gloomy::gfx::DeviceOptions options;
  options.app_name = "gloomy_bench";
  options.validation = validation;
  auto device = gloomy::gfx::DeviceContext::Create(options);
  if (!device) {
    std::fprintf(stderr, "%s\n", device.error().c_str());
    return 1;
  }
  const auto setup_start = Clock::now();
  Bench bench(**device, rows, size);
  std::printf("gloomy_bench: %d rows, %ux%u, %s, setup %.0f ms\n\n",
              rows,
              size.width,
              size.height,
              (*device)->device_name().c_str(),
              Ms(Clock::now() - setup_start));

  // Warm up: glyphs, shape cache, pipelines.
  for (int i = 0; i < 10; ++i) {
    bench.Frame();
  }

  std::printf(
      "| Scenario                     | Frames | UI avg | UI p95 | "
      "UI max | GPU avg | GPU p95 |\n");
  std::printf(
      "|------------------------------|-------:|-------:|-------:|"
      "-------:|--------:|--------:|\n");

  Samples idle;
  for (int i = 0; i < 120; ++i) {
    auto [ui, gpu] = bench.Frame();
    idle.ui.push_back(ui);
    idle.gpu.push_back(gpu);
  }
  idle.Print("Idle (1M rows)");

  bench.Focus("##grid");
  Samples page;
  for (int i = 0; i < 300; ++i) {
    auto [ui, gpu] = bench.Frame({InputEvent::KeyDown(Key::kPageDown)});
    page.ui.push_back(ui);
    page.gpu.push_back(gpu);
  }
  page.Print("Page Down x300");

  Samples wheel;
  for (int i = 0; i < 300; ++i) {
    auto [ui, gpu] = bench.Frame({InputEvent::Move(600, 400),
                                  InputEvent::Wheel(0, i % 100 < 50 ? -3 : 3)});
    wheel.ui.push_back(ui);
    wheel.gpu.push_back(gpu);
  }
  wheel.Print("Wheel scrolling x300");

  Samples jump;
  for (int i = 0; i < 100; ++i) {
    auto [ui, gpu] =
        bench.Frame({InputEvent::KeyDown(i % 2 == 0 ? Key::kEnd : Key::kHome)});
    jump.ui.push_back(ui);
    jump.gpu.push_back(gpu);
  }
  jump.Print("Home/End jumps x100");

  // Typing a search, one character per frame burst, waiting for each.
  bench.FocusSearch();
  Samples typing;
  Latency type_latency;
  const std::string query = "ore belt 12";
  for (const char c : query) {
    const auto t0 = Clock::now();
    bench.Frame({InputEvent::Text(std::string(1, c))});
    const int frames = WaitForView(bench, typing);
    type_latency.ms.push_back(Ms(Clock::now() - t0));
    type_latency.frames.push_back(frames);
  }
  typing.Print("Typing a search (frames)");
  Samples erasing;
  Latency erase_latency;
  for (std::size_t i = 0; i < query.size(); ++i) {
    const auto t0 = Clock::now();
    bench.Frame({InputEvent::KeyDown(Key::kBackspace)});
    const int frames = WaitForView(bench, erasing);
    erase_latency.ms.push_back(Ms(Clock::now() - t0));
    erase_latency.frames.push_back(frames);
  }
  erasing.Print("Backspace (frames)");

  // Sorting by different columns.
  Samples sorting;
  Latency sort_latency;
  const int columns[] = {gloomy::apps::kPrice,
                         gloomy::apps::kStation,
                         gloomy::apps::kStock,
                         gloomy::apps::kSector};
  for (const int col : columns) {
    for (const auto dir : {gloomy::data::SortDirection::kAscending,
                           gloomy::data::SortDirection::kDescending}) {
      bench.grid().query.sort = {{col, dir}};
      const auto t0 = Clock::now();
      const int frames = WaitForView(bench, sorting);
      sort_latency.ms.push_back(Ms(Clock::now() - t0));
      sort_latency.frames.push_back(frames);
    }
  }
  bench.grid().query.sort = {
      {gloomy::apps::kGood, gloomy::data::SortDirection::kAscending},
      {gloomy::apps::kPrice, gloomy::data::SortDirection::kDescending}};
  {
    const auto t0 = Clock::now();
    const int frames = WaitForView(bench, sorting);
    sort_latency.ms.push_back(Ms(Clock::now() - t0));
    sort_latency.frames.push_back(frames);
  }
  sorting.Print("Sorting (frames)");

  std::printf(
      "\n| Query latency                | Count | Avg ms   | Max ms "
      "  | Frames   |\n");
  std::printf(
      "|------------------------------|------:|---------:|---------:|"
      "---------:|\n");
  type_latency.Print("Search, per keystroke");
  erase_latency.Print("Backspace, per keystroke");
  sort_latency.Print("Sort, per change");
  std::printf("\nInstances in the last frame: %u\n", bench.instances());
  return (*device)->validation_message_count() == 0 ? 0 : 2;
}
