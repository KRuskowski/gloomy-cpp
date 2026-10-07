#ifndef GLOOMY_APPS_DEMO_DEMO_UI_H_
#define GLOOMY_APPS_DEMO_DEMO_UI_H_

#include <string>

#include "gloomy/ui/context.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::demo {

/// State of the demo UI, owned by the app.
struct DemoState {
  bool gates = true;
  bool traders = false;
  int mode = 1;
  float zoom = 0.4F;
  std::string search;
  int good = 1;
  int tab = 0;
  int theme = 0;
  ui::ListState list;
  int activated = -1;
};

/// Declares the demo UI for one frame.
void DemoUi(ui::Context &ctx, DemoState &state);

}  // namespace gloomy::demo

#endif  // GLOOMY_APPS_DEMO_DEMO_UI_H_
