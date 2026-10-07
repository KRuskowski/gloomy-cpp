// gloomy_demo: the widget gallery in a window, or headless into a PNG.
//
//   gloomy_demo                          open a window (SDL2)
//   gloomy_demo --screenshot out.png     render offscreen and exit
//   gloomy_demo --size 960x600           window or screenshot size

#include "demo_ui.h"
#include "gloomy/app/app_shell.h"
#include "gloomy/ui/theme.h"

auto main(int argc, char **argv) -> int {
  gloomy::demo::DemoState state;
  gloomy::app::ShellOptions options;
  options.title = "gloomy demo";
  options.size = {960, 600};
  options.frame = [&](gloomy::ui::Context &ctx,
                      const gloomy::app::FrameInfo &info) {
    const char *names[] = {"Dark", "Light", "High Contrast"};
    if (ctx.theme().name != names[state.theme]) {
      ctx.SetTheme(state.theme == 1 ? gloomy::ui::Theme::Light(info.fonts)
                   : state.theme == 2
                       ? gloomy::ui::Theme::HighContrast(info.fonts)
                       : gloomy::ui::Theme::Dark(info.fonts));
    }
    gloomy::demo::DemoUi(ctx, state);
  };
  gloomy::app::ParseShellArgs(argc, argv, options);
  return gloomy::app::RunApp(options);
}
