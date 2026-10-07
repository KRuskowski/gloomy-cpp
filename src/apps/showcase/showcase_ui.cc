#include "showcase_ui.h"

#include <cmath>
#include <cstdio>

#include "gloomy/ui/summary.h"
#include "gloomy/ui/theme.h"
#include "gloomy/ui/view_preset.h"

namespace gloomy::showcase {

namespace {

using apps::MarketColumn;
using ui::LayoutSpec;
using ui::SizeSpec;

auto History(double base, int seed) -> std::vector<float> {
  std::vector<float> v;
  double x = base;
  std::uint32_t s = static_cast<std::uint32_t>(seed) * 2654435761U;
  for (int i = 0; i < 24; ++i) {
    s = s * 1664525U + 1013904223U;
    x += (static_cast<double>(s % 1000) / 1000.0 - 0.48) * base * 0.04;
    v.push_back(static_cast<float>(x));
  }
  return v;
}

auto Preset(std::string name, std::string description, data::Query query)
    -> ui::ViewPreset {
  ui::ViewPreset p;
  p.name = std::move(name);
  p.description = std::move(description);
  p.query = std::move(query);
  p.curated = true;
  return p;
}

auto Filter(int column, data::FilterOp op, double a, std::string text = {})
    -> data::Filter {
  data::Filter f;
  f.column = column;
  f.op = op;
  f.a = a;
  f.text = std::move(text);
  return f;
}

void ApplyActivePreset(ShowcaseState &s) {
  if (s.preset >= 0 && s.preset < static_cast<int>(s.presets.size())) {
    ui::ApplyPreset(s.presets[static_cast<std::size_t>(s.preset)], s.grid);
    s.grid.query.search = s.search;
  }
}

void Toast(ShowcaseState &s, std::string text,
           ui::Tone tone = ui::Tone::kNeutral) {
  s.toasts.Show(std::move(text), tone);
}

// Drills down from an answer or card into the table behind it.
void DrillTo(ui::Context &ctx, ShowcaseState &s, int preset) {
  s.preset = preset;
  s.tab = 0;
  ApplyActivePreset(s);
  s.grid.cursor.reset();
  s.grid.scroll_y = 0.0;
  ctx.SetFocus(ctx.GetId("##market"));
}

void Header(ui::Context &ctx, const app::FrameInfo &info, ShowcaseState &s) {
  static const std::vector<std::string> kThemes{
      "Dark", "Light", "High contrast"};
  LayoutSpec row;
  row.gap = 12;
  row.align = ui::Align::kCenter;
  row.width = SizeSpec::Grow();
  ui::BeginRow(ctx, row);
  ui::Heading(ctx, "X-conomy \xC2\xB7 Markets");
  ui::LabelOptions muted;
  muted.muted = true;
  ui::Label(ctx, "gloomy-cpp showcase", muted);
  ui::Spacer(ctx);
  if (ui::Dropdown(ctx, "Theme", s.theme, kThemes)) {
    ctx.SetTheme(s.theme == 1   ? ui::Theme::Light(info.fonts)
                 : s.theme == 2 ? ui::Theme::HighContrast(info.fonts)
                                : ui::Theme::Dark(info.fonts));
  }
  ui::End(ctx);
}

void Answer(ui::Context &ctx, ShowcaseState &s) {
  s.answer_view->Update(ctx.time());
  const auto rows = s.answer_view->rows();
  const data::TableSource &src = *s.market;
  std::string answer = "Looking for the cheapest ore\xE2\x80\xA6";
  std::string reason = "Searching 1,000,000 market reports";
  if (!rows.empty() && !s.answer_view->busy()) {
    const data::RowIndex r = rows[0];
    std::string price;
    src.FormatCell(r, apps::kPrice, price);
    answer = "Cheapest ore: " + std::string(src.GetText(apps::kStation, r)) +
             " in " + std::string(src.GetText(apps::kSector, r)) + " at " +
             price;
    data::Explanation why;
    if (src.Explain(r, apps::kPrice, why)) {
      reason = why.reason + " \xC2\xB7 report " +
               std::to_string(static_cast<int>(src.GetDouble(apps::kAge, r))) +
               " min old";
    }
  }
  if (ui::AnswerLine(ctx, "##answer", answer, reason, "Show all ore offers")) {
    DrillTo(ctx, s, 1);
  }
}

void Cards(ui::Context &ctx, ShowcaseState &s) {
  LayoutSpec row;
  row.gap = 10;
  row.width = SizeSpec::Grow();
  ui::BeginRow(ctx, row);
  char value[64];
  {
    ui::SummarySpec spec;
    spec.title = "Market reports";
    std::string count;
    data::FormatInt(s.market->RowCount(), true, count);
    spec.value = {count, "10 sectors"};
    if (ui::SummaryCard(ctx, "##reports", spec)) {
      DrillTo(ctx, s, 0);
    }
  }
  {
    ui::SummarySpec spec;
    spec.title = "Ore, average price";
    std::snprintf(value, sizeof(value), "%.2f Cr", s.avg_ore);
    spec.value = {value, "across all sellers"};
    spec.delta = "-3.8% today";
    spec.delta_tone = ui::Tone::kGood;
    spec.sparkline = s.ore_history;
    if (ui::SummaryCard(ctx, "##ore", spec)) {
      DrillTo(ctx, s, 1);
    }
  }
  {
    ui::SummarySpec spec;
    spec.title = "Energy cells, average price";
    std::snprintf(value, sizeof(value), "%.2f Cr", s.avg_energy);
    spec.value = {value, "stable"};
    spec.delta = "+0.4% today";
    spec.sparkline = s.energy_history;
    if (ui::SummaryCard(ctx, "##energy", spec)) {
      DrillTo(ctx, s, 0);
    }
  }
  {
    ui::SummarySpec spec;
    spec.title = "Shortages";
    std::string count;
    data::FormatInt(s.shortages, true, count);
    spec.value = {count, "sellers under 500 units"};
    spec.status = ui::Tone::kWarning;
    spec.delta = "needs attention";
    spec.delta_tone = ui::Tone::kWarning;
    if (ui::SummaryCard(ctx, "##shortages", spec)) {
      DrillTo(ctx, s, 2);
    }
  }
  ui::End(ctx);
}

void MarketsTab(ui::Context &ctx, ShowcaseState &s) {
  const ui::PresetBarResult bar =
      ui::PresetBar(ctx, "##presets", s.presets, s.preset);
  if (bar.changed) {
    ApplyActivePreset(s);
  }
  if (bar.share) {
    ctx.clipboard().set(ui::EncodePreset(
        ui::CapturePreset(
            s.preset >= 0 ? s.presets[static_cast<std::size_t>(s.preset)].name
                          : "My view",
            s.grid),
        *s.market));
    Toast(
        s, "View copied to the clipboard; paste it to share.", ui::Tone::kGood);
  }
  if (bar.save) {
    ui::ViewPreset p = ui::CapturePreset(
        "My view " + std::to_string(s.presets.size() - 4), s.grid);
    s.presets.push_back(std::move(p));
    s.preset = static_cast<int>(s.presets.size()) - 1;
    Toast(s, "Saved as \"" + s.presets.back().name + "\".");
  }

  std::string status;
  data::FormatInt(
      static_cast<std::int64_t>(s.view->rows().size()), true, status);
  status += " rows";
  ui::SearchFieldOptions opt;
  opt.placeholder = "Search stations, sectors, goods, owners  (Ctrl+F)";
  opt.status = status;
  opt.busy = s.view->busy();
  const ui::SearchFieldResult r =
      ui::SearchField(ctx, "##market_search", s.search, opt);
  if (r.changed) {
    s.grid.query.search = s.search;
  }
  if (r.next) {
    ctx.SetFocus(ctx.GetId("##market"));
  }
  LayoutSpec grid_layout;
  grid_layout.height = SizeSpec::Grow();
  const ui::GridResult g =
      ui::DataGrid(ctx, "##market", *s.view, s.grid, grid_layout);
  if (ui::GridCellMenu(ctx, "##cell_menu", *s.view, s.grid, g) ||
      g.query_changed) {
    s.preset = -1;
  }
  if (g.activated && g.row) {
    Toast(s,
          "Plot course to " +
              std::string(s.market->GetText(apps::kStation, *g.row)) + ".");
  }
}

// Panels: the level between the glance cards and the full table.
void OverviewTab(ui::Context &ctx, ShowcaseState &s) {
  s.shortage_view->Update(ctx.time());
  LayoutSpec outer;
  outer.gap = 10;
  outer.width = SizeSpec::Grow();
  outer.align = ui::Align::kStretch;
  ui::BeginColumn(ctx, outer);
  LayoutSpec row;
  row.gap = 10;
  row.width = SizeSpec::Grow();
  ui::BeginRow(ctx, row);
  {
    char avg[32];
    std::snprintf(avg, sizeof(avg), "%.2f Cr", s.avg_ore);
    std::string offers;
    data::FormatInt(
        static_cast<std::int64_t>(s.answer_view->rows().size()), true, offers);
    const ui::KeyNumber numbers[] = {
        {"Ore offers", {offers, ""}},
        {"Average price", {avg, "Mean of all ore offers"}, ui::Tone::kNeutral}};
    const int columns[] = {
        apps::kStation, apps::kSector, apps::kStock, apps::kPrice};
    ui::SummaryPanelSpec spec;
    spec.title = "Cheapest ore";
    spec.numbers = numbers;
    spec.columns = columns;
    spec.rows = 6;
    spec.action = "All ore offers";
    if (ui::SummaryPanel(ctx, "##ore_panel", *s.answer_view, spec)) {
      DrillTo(ctx, s, 1);
    }
  }
  {
    std::string count;
    data::FormatInt(s.shortages, true, count);
    const ui::KeyNumber numbers[] = {{"Markets short",
                                      {count, "Under 500 units in stock"},
                                      ui::Tone::kWarning}};
    const int columns[] = {
        apps::kStation, apps::kGood, apps::kStock, apps::kPrice};
    ui::SummaryPanelSpec spec;
    spec.title = "Shortages";
    spec.numbers = numbers;
    spec.columns = columns;
    spec.rows = 6;
    spec.action = "All shortages";
    if (ui::SummaryPanel(ctx, "##shortage_panel", *s.shortage_view, spec)) {
      DrillTo(ctx, s, 2);
    }
  }
  ui::End(ctx);
  // Price history, like the current client's ImPlot charts.
  const ui::ChartSeries series[] = {
      {"Ore", s.days, s.ore_index},
      {"Energy cells", s.days, s.energy_index},
      {"Silicon wafers", s.days, s.silicon_index}};
  ui::ChartOptions chart;
  chart.x_label = "day";
  chart.y_label = "% of base price";
  chart.height = SizeSpec::Fixed(230);
  ui::LineChart(ctx, "##price_chart", series, s.chart, chart);
  ui::End(ctx);
}

void ContractTab(ui::Context &ctx, ShowcaseState &s) {
  LayoutSpec row;
  row.gap = 10;
  row.width = SizeSpec::Grow();
  row.height = SizeSpec::Grow();
  ui::BeginRow(ctx, row);
  LayoutSpec side;
  side.width = SizeSpec::Fixed(220);
  side.height = SizeSpec::Grow();
  side.gap = 6;
  ui::BeginColumn(ctx, side);
  ui::LabelOptions title;
  title.muted = true;
  ui::Label(ctx, "Sections", title);
  LayoutSpec list_layout;
  list_layout.height = SizeSpec::Grow();
  const ui::ListResult lr = ui::List(
      ctx,
      "##outline",
      s.outline_list,
      static_cast<int>(s.outline.size()),
      [&](int i) -> std::string_view {
        return s.outline[static_cast<std::size_t>(i)].title;
      },
      list_layout);
  if ((lr.selection_changed || lr.activated) && s.outline_list.selected >= 0) {
    ui::ScrollToBlock(
        s.doc,
        s.outline[static_cast<std::size_t>(s.outline_list.selected)].block);
  }
  ui::End(ctx);

  LayoutSpec main;
  main.width = SizeSpec::Grow();
  main.height = SizeSpec::Grow();
  main.gap = 8;
  ui::BeginColumn(ctx, main);
  ui::SearchFieldOptions opt;
  opt.placeholder = "Search the contract  (Ctrl+F, F3 next)";
  const std::string status = ui::MatchStatus(s.doc);
  opt.status = status;
  const ui::SearchFieldResult r =
      ui::SearchField(ctx, "##doc_search", s.doc.search, opt);
  if (r.next) {
    ui::GoToMatch(s.doc, false);
  }
  if (r.previous) {
    ui::GoToMatch(s.doc, true);
  }
  const ui::DocumentResult d =
      ui::DocumentView(ctx, "##contract", s.contract, s.doc);
  if (d.link) {
    Toast(s, "Open " + *d.link);
  }
  ui::End(ctx);
  ui::End(ctx);
}

}  // namespace

void InitShowcase(ShowcaseState &s, int rows) {
  s.market = std::make_shared<apps::SampleMarket>(rows);
  s.view = std::make_unique<data::TableView>(s.market);
  s.answer_view = std::make_unique<data::TableView>(s.market);
  s.grid.columns = ui::DefaultColumns(*s.market);
  s.grid.columns[apps::kStation].pinned = true;
  s.grid.columns[apps::kStation].width = 130;
  s.grid.columns[apps::kStation].renderer = ui::CellRenderer::kAge;
  s.grid.columns[apps::kStation].age_column = apps::kAge;
  s.grid.columns[apps::kStation].age_fresh = 30;
  s.grid.columns[apps::kStation].age_stale = 220;
  s.grid.columns[apps::kSector].width = 150;
  s.grid.columns[apps::kGood].width = 130;
  s.grid.columns[apps::kStock].renderer = ui::CellRenderer::kBar;
  s.grid.columns[apps::kStock].bar_max = 50000;
  s.grid.columns[apps::kAge].width = 100;

  data::Query all;
  data::Query ore;
  ore.filters = {Filter(apps::kGood, data::FilterOp::kTextEquals, 0, "Ore")};
  ore.sort = {{apps::kPrice, data::SortDirection::kAscending}};
  data::Query shortages;
  shortages.filters = {Filter(apps::kStock, data::FilterOp::kLess, 500)};
  shortages.sort = {{apps::kStock, data::SortDirection::kAscending}};
  data::Query fresh;
  fresh.filters = {Filter(apps::kAge, data::FilterOp::kLess, 10)};
  fresh.sort = {{apps::kAge, data::SortDirection::kAscending}};
  data::Query player;
  player.filters = {
      Filter(apps::kOwner, data::FilterOp::kTextEquals, 0, "Player")};
  player.sort = {{apps::kSector, data::SortDirection::kAscending},
                 {apps::kStation, data::SortDirection::kAscending}};
  s.presets = {
      Preset("All markets", "Every market report you have", all),
      Preset("Cheapest ore", "Ore offers, cheapest first", ore),
      Preset("Shortages", "Markets with under 500 units in stock", shortages),
      Preset("Fresh reports", "Reports younger than 10 minutes", fresh),
      Preset("My stations", "Stations you own", player),
  };
  s.answer_view->SetQuery(ore);
  s.shortage_view = std::make_unique<data::TableView>(s.market);
  s.shortage_view->SetQuery(shortages);
  ApplyActivePreset(s);

  s.avg_ore = s.market->AveragePrice("Ore");
  s.avg_energy = s.market->AveragePrice("Energy Cells");
  for (int i = 0; i <= 600; ++i) {
    const double d = i / 6.0;
    s.days.push_back(d);
    s.ore_index.push_back(100 + 14 * std::sin(d * 0.21) +
                          4 * std::sin(d * 1.7 + 1));
    s.energy_index.push_back(100 + 5 * std::cos(d * 0.13) +
                             2 * std::sin(d * 2.3));
    s.silicon_index.push_back(92 + 0.12 * d + 6 * std::sin(d * 0.37 + 2));
  }
  s.ore_history = History(s.avg_ore, 1);
  s.energy_history = History(s.avg_energy, 2);
  for (data::RowIndex r = 0; r < s.market->RowCount(); ++r) {
    s.shortages += s.market->GetInt64(apps::kStock, r) < 500 ? 1 : 0;
  }

  s.dock.Dock("contract", "markets", ui::DockSide::kRight, 0.4F);
  s.dock.Dock("overview", "contract", ui::DockSide::kBottom, 0.55F);
  s.contract = data::Document::ParseMarkdown(apps::SampleContract(400));
  s.outline = s.contract.Outline();

  ShowcaseState *state = &s;
  s.commands = std::make_unique<ui::CommandProvider>();
  s.commands->Add({"Go to markets", "Ctrl+1", [state] {
                     state->tab = 0;
                   }});
  s.commands->Add({"Go to contract", "Ctrl+2", [state] {
                     state->tab = 1;
                   }});
  s.commands->Add({"Search markets", "Ctrl+F", [state] {
                     state->tab = 0;
                     state->focus_request = "##market_search";
                   }});
  s.commands->Add({"Search the contract", "", [state] {
                     state->tab = 1;
                     state->focus_request = "##doc_search";
                   }});
  for (std::size_t i = 0; i < s.presets.size(); ++i) {
    s.commands->Add({"View: " + s.presets[i].name, "", [state, i] {
                       state->preset_request = static_cast<int>(i);
                     }});
  }
  s.commands->Add({"Theme: dark", "", [state] {
                     state->theme_request = 0;
                   }});
  s.commands->Add({"Theme: light", "", [state] {
                     state->theme_request = 1;
                   }});
  s.commands->Add({"Theme: high contrast", "", [state] {
                     state->theme_request = 2;
                   }});
  s.station_search = std::make_unique<ui::TableProvider>(
      "Market reports",
      s.market,
      apps::kStation,
      std::vector<int>{apps::kSector, apps::kGood, apps::kPrice},
      [state](data::RowIndex row) {
        state->tab = 0;
        state->preset = 0;
        state->search.clear();
        ApplyActivePreset(*state);
        state->grid.cursor = state->market->RowKey(row);
        state->grid.reveal = true;
        state->focus_request = "##market";
      });
  s.contract_search = std::make_unique<ui::DocumentProvider>(
      "Contract AFM-0042", s.contract, [state](const data::DocMatch &m) {
        state->tab = 1;
        ui::ScrollToBlock(state->doc, m.block);
      });
}

void ShowcaseUi(ui::Context &ctx, const app::FrameInfo &info,
                ShowcaseState &s) {
  const ui::Theme &t = ctx.theme();
  // Requests from commands and search hits.
  if (s.theme_request >= 0) {
    s.theme = s.theme_request;
    s.theme_request = -1;
    ctx.SetTheme(s.theme == 1   ? ui::Theme::Light(info.fonts)
                 : s.theme == 2 ? ui::Theme::HighContrast(info.fonts)
                                : ui::Theme::Dark(info.fonts));
  }
  if (s.preset_request >= 0) {
    DrillTo(ctx, s, s.preset_request);
    s.preset_request = -1;
  }
  if (!s.focus_request.empty()) {
    ctx.SetFocus(s.focus_request == "##market"
                     ? ctx.GetId("##market")
                     : ui::SearchFieldId(ctx, s.focus_request));
    s.focus_request.clear();
  }
  // Ctrl+K searches everything, Ctrl+P runs commands.
  if (ctx.ConsumeKey(ui::Key::kK, ui::kModCtrl)) {
    s.finder.open = true;
    s.palette_only = false;
  }
  if (ctx.ConsumeKey(ui::Key::kP, ui::kModCtrl)) {
    s.finder.open = true;
    s.palette_only = true;
  }
  // Global keys: Ctrl+F searches the current tab, Ctrl+1/2 switch tabs.
  if (ctx.ConsumeKey(ui::Key::kF, ui::kModCtrl) ||
      ctx.ConsumeKey(ui::Key::kSlash)) {
    ctx.SetFocus(ui::SearchFieldId(
        ctx, s.tab == 0 ? "##market_search" : "##doc_search"));
  }
  if (ctx.ConsumeKey(ui::Key::k1, ui::kModCtrl)) {
    s.tab = 0;
  }
  if (ctx.ConsumeKey(ui::Key::k2, ui::kModCtrl)) {
    s.tab = 1;
  }

  LayoutSpec root;
  root.padding = ui::Padding::All(12);
  root.gap = 10;
  root.width = SizeSpec::Grow();
  root.height = SizeSpec::Grow();
  root.align = ui::Align::kStretch;
  ui::BeginColumn(ctx, root);
  Header(ctx, info, s);
  Answer(ctx, s);
  Cards(ctx, s);
  static const std::vector<std::string> kTabs{
      "Markets", "Contract AFM-0042", "Overview", "Workspace"};
  ui::Tabs(ctx, "##tabs", s.tab, kTabs);
  if (s.tab == 0) {
    MarketsTab(ctx, s);
  } else if (s.tab == 1) {
    ContractTab(ctx, s);
  } else if (s.tab == 2) {
    OverviewTab(ctx, s);
  } else {
    // The same screens as docked panels: drag tabs to rearrange.
    ui::DockSpace(
        ctx,
        "##workspace",
        s.dock,
        s.dock_state,
        [&s](ui::Context &c, const std::string &panel) {
          if (panel == "markets") {
            MarketsTab(c, s);
          } else if (panel == "contract") {
            ContractTab(c, s);
          } else {
            OverviewTab(c, s);
          }
        },
        [](const std::string &panel) -> std::string {
          return panel == "markets"    ? "Markets"
                 : panel == "contract" ? "Contract"
                                       : "Overview";
        },
        false);
  }
  // Status line.
  char status[160];
  std::snprintf(status,
                sizeof(status),
                "UI %.2f ms \xC2\xB7 frame %.1f ms \xC2\xB7 %s \xC2\xB7 "
                "Ctrl+K search everything, Ctrl+P commands, Ctrl+F search here",
                info.ui_ms,
                info.frame_ms,
                info.device_name.c_str());
  ui::LabelOptions muted;
  muted.muted = true;
  muted.size = t.metrics.small_font_size;
  ui::Label(ctx, status, muted);
  ui::End(ctx);

  if (s.finder.open) {
    if (s.palette_only) {
      ui::SearchProvider *providers[] = {s.commands.get()};
      ui::Finder(ctx, "##finder", providers, s.finder, "Run a command");
    } else {
      ui::SearchProvider *providers[] = {
          s.commands.get(), s.station_search.get(), s.contract_search.get()};
      ui::Finder(ctx,
                 "##finder",
                 providers,
                 s.finder,
                 "Search stations, goods, the contract and commands");
    }
  }

  ui::Toasts(ctx, s.toasts);
}

}  // namespace gloomy::showcase
