#ifndef GLOOMY_APPS_SHOWCASE_SHOWCASE_UI_H_
#define GLOOMY_APPS_SHOWCASE_SHOWCASE_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "gloomy/app/app_shell.h"
#include "gloomy/data/document.h"
#include "gloomy/data/table_view.h"
#include "gloomy/ui/chart.h"
#include "gloomy/ui/data_grid.h"
#include "gloomy/ui/dock.h"
#include "gloomy/ui/document_view.h"
#include "gloomy/ui/finder.h"
#include "gloomy/ui/overlays.h"
#include "gloomy/ui/view_preset.h"
#include "gloomy/ui/widgets.h"
#include "sample_data.h"

namespace gloomy::showcase {

/// Everything the showcase keeps between frames.
struct ShowcaseState {
  std::shared_ptr<apps::SampleMarket> market;
  std::unique_ptr<data::TableView> view;
  /// The query behind the answer line (row one is the answer).
  std::unique_ptr<data::TableView> answer_view;
  /// Behind the "Shortages" panel.
  std::unique_ptr<data::TableView> shortage_view;
  ui::GridState grid;
  std::vector<ui::ViewPreset> presets;
  int preset = 0;
  std::string search;
  data::Document contract;
  std::vector<data::DocSection> outline;
  ui::DocumentViewState doc;
  ui::ListState outline_list;
  int tab = 0;
  int theme = 0;
  // Price index history for the overview chart.
  std::vector<double> days;
  std::vector<double> ore_index;
  std::vector<double> energy_index;
  std::vector<double> silicon_index;
  ui::ChartState chart;
  std::vector<float> ore_history;
  std::vector<float> energy_history;
  double avg_ore = 0.0;
  double avg_energy = 0.0;
  std::int64_t shortages = 0;
  ui::ToastQueue toasts;
  // Global search (Ctrl+K) and the command palette (Ctrl+P).
  ui::FinderState finder;
  bool palette_only = false;
  std::unique_ptr<ui::CommandProvider> commands;
  std::unique_ptr<ui::TableProvider> station_search;
  std::unique_ptr<ui::DocumentProvider> contract_search;
  // The docked workspace tab: the same panels, arranged freely.
  ui::DockLayout dock{{"markets"}};
  ui::DockState dock_state;
  // Work requested by commands, done in the next frame.
  std::string focus_request;
  int theme_request = -1;
  int preset_request = -1;
};

/// Builds the sample data, presets and views.
void InitShowcase(ShowcaseState &state, int rows);

/// Declares the showcase UI for one frame.
void ShowcaseUi(ui::Context &ctx, const app::FrameInfo &info,
                ShowcaseState &state);

}  // namespace gloomy::showcase

#endif  // GLOOMY_APPS_SHOWCASE_SHOWCASE_UI_H_
