// gloomy_showcase: an X-conomy-style market screen over a million rows,
// and a long contract with search.
//
//   gloomy_showcase [--rows N] [--screenshot out.png] [--size WxH]

#include <cstdlib>
#include <cstring>
#include <string>

#include "app_shell.h"
#include "showcase_ui.h"

auto main(int argc, char **argv) -> int {
  int rows = 1000000;
  int tab = 0;
  std::string finder_query;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--rows") == 0 && i + 1 < argc) {
      rows = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--tab") == 0 && i + 1 < argc) {
      tab = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--finder") == 0 && i + 1 < argc) {
      // Screenshots of the finder: open it with this query.
      finder_query = argv[++i];
    }
  }
  gloomy::showcase::ShowcaseState state;
  gloomy::showcase::InitShowcase(state, rows);
  state.tab = tab;
  if (!finder_query.empty()) {
    state.finder.open = true;
    state.finder.query = finder_query;
  }
  gloomy::apps::ShellOptions options;
  options.title = "gloomy showcase";
  options.screenshot_frames = 6;
  options.frame = [&](gloomy::ui::Context &ctx,
                      const gloomy::apps::FrameInfo &info) {
    gloomy::showcase::ShowcaseUi(ctx, info, state);
  };
  gloomy::apps::ParseShellArgs(argc, argv, options);
  options.settled = [&] {
    return !state.view->busy() && !state.answer_view->busy() &&
           !state.shortage_view->busy() && !state.station_search->busy();
  };
  return gloomy::apps::RunApp(options);
}
