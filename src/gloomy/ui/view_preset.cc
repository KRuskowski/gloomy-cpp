#include "gloomy/ui/view_preset.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <optional>

#include "gloomy/ui/disclosure.h"
#include "gloomy/ui/overlays.h"
#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

constexpr std::string_view kMagic = "gloomy-view:1";

struct OpName {
  data::FilterOp op;
  std::string_view name;
};

constexpr OpName kOps[] = {
    {data::FilterOp::kEqual, "eq"},
    {data::FilterOp::kNotEqual, "ne"},
    {data::FilterOp::kLess, "lt"},
    {data::FilterOp::kLessEqual, "le"},
    {data::FilterOp::kGreater, "gt"},
    {data::FilterOp::kGreaterEqual, "ge"},
    {data::FilterOp::kBetween, "between"},
    {data::FilterOp::kContains, "contains"},
    {data::FilterOp::kNotContains, "notcontains"},
    {data::FilterOp::kStartsWith, "starts"},
    {data::FilterOp::kEndsWith, "ends"},
    {data::FilterOp::kTextEquals, "is"},
    {data::FilterOp::kInSet, "in"},
    {data::FilterOp::kBlank, "blank"},
    {data::FilterOp::kNotBlank, "notblank"},
};

auto Escape(std::string_view s) -> std::string {
  std::string out;
  for (const char c : s) {
    if (c == ';' || c == '=' || c == ',' || c == ':' || c == '|' || c == '%' ||
        c == ' ' || c == '\n') {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      out += buf;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

auto Unescape(std::string_view s) -> std::string {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      unsigned v = 0;
      std::from_chars(s.data() + i + 1, s.data() + i + 3, v, 16);
      out.push_back(static_cast<char>(v));
      i += 2;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

auto Split(std::string_view s, char sep) -> std::vector<std::string_view> {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (start <= s.size()) {
    const std::size_t end = s.find(sep, start);
    if (end == std::string_view::npos) {
      parts.push_back(s.substr(start));
      break;
    }
    parts.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

auto Number(double v) -> std::string {
  char buf[32];
  const auto r = std::to_chars(buf, buf + sizeof(buf), v);
  return {buf, r.ptr};
}

auto ParseNumber(std::string_view s) -> double {
  double v = 0.0;
  std::from_chars(s.data(), s.data() + s.size(), v);
  return v;
}

}  // namespace

void ApplyPreset(const ViewPreset &preset, GridState &state) {
  state.query = preset.query;
  if (preset.columns.empty()) {
    return;
  }
  std::vector<GridColumn> columns;
  for (const GridColumn &p : preset.columns) {
    GridColumn c = p;
    for (const GridColumn &old : state.columns) {
      if (old.column == p.column) {
        c = old;
        c.width = p.width;
        c.visible = p.visible;
        c.pinned = p.pinned;
      }
    }
    columns.push_back(c);
  }
  for (const GridColumn &old : state.columns) {
    const bool listed =
        std::any_of(columns.begin(), columns.end(), [&](const GridColumn &c) {
          return c.column == old.column;
        });
    if (!listed) {
      GridColumn c = old;
      c.visible = false;
      columns.push_back(c);
    }
  }
  std::stable_partition(columns.begin(),
                        columns.end(),
                        [](const GridColumn &c) { return c.pinned; });
  state.columns = std::move(columns);
}

auto CapturePreset(std::string name, const GridState &state) -> ViewPreset {
  ViewPreset p;
  p.name = std::move(name);
  p.query = state.query;
  p.columns = state.columns;
  return p;
}

auto EncodePreset(const ViewPreset &p, const data::TableSource &source)
    -> std::string {
  const auto infos = source.Columns();
  auto name_of = [&](int col) {
    return Escape(infos[col].name);
  };
  std::string out(kMagic);
  out += ";n=" + Escape(p.name);
  if (!p.description.empty()) {
    out += ";d=" + Escape(p.description);
  }
  if (!p.query.sort.empty()) {
    out += ";s=";
    for (std::size_t i = 0; i < p.query.sort.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      out += name_of(p.query.sort[i].column);
      out += p.query.sort[i].direction == data::SortDirection::kAscending
                 ? ":a"
                 : ":d";
    }
  }
  for (const data::Filter &f : p.query.filters) {
    out += ";f=" + name_of(f.column) + ":";
    for (const OpName &o : kOps) {
      if (o.op == f.op) {
        out += o.name;
      }
    }
    out += ":" + Number(f.a) + ":" + Number(f.b) + ":" + Escape(f.text) + ":";
    for (std::size_t i = 0; i < f.set.size(); ++i) {
      if (i > 0) {
        out += '|';
      }
      out += std::to_string(f.set[i]);
    }
  }
  if (!p.query.search.empty()) {
    out += ";q=" + Escape(p.query.search);
  }
  if (!p.query.group_by.empty()) {
    out += ";g=";
    for (std::size_t i = 0; i < p.query.group_by.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      out += name_of(p.query.group_by[i]);
    }
  }
  if (!p.query.aggregates.empty()) {
    static constexpr const char *kAggNames[] = {
        "count", "sum", "min", "max", "mean"};
    out += ";a=";
    for (std::size_t i = 0; i < p.query.aggregates.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      out += name_of(p.query.aggregates[i].column) + ":" +
             kAggNames[static_cast<int>(p.query.aggregates[i].op)];
    }
  }
  if (!p.columns.empty()) {
    out += ";c=";
    for (std::size_t i = 0; i < p.columns.size(); ++i) {
      const GridColumn &c = p.columns[i];
      if (i > 0) {
        out += ',';
      }
      out += name_of(c.column) + ":" + Number(c.width) + ":";
      out += c.visible ? "" : "h";
      out += c.pinned ? "p" : "";
    }
  }
  return out;
}

auto DecodePreset(std::string_view text, const data::TableSource &source)
    -> std::optional<ViewPreset> {
  while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
    text.remove_suffix(1);
  }
  if (!text.starts_with(kMagic)) {
    return std::nullopt;
  }
  ViewPreset p;
  auto column = [&](std::string_view name) {
    return source.FindColumn(Unescape(name));
  };
  for (std::string_view part : Split(text.substr(kMagic.size()), ';')) {
    if (part.size() < 2 || part[1] != '=') {
      continue;
    }
    const char key = part[0];
    const std::string_view value = part.substr(2);
    if (key == 'n') {
      p.name = Unescape(value);
    } else if (key == 'd') {
      p.description = Unescape(value);
    } else if (key == 'q') {
      p.query.search = Unescape(value);
    } else if (key == 'g') {
      for (std::string_view g : Split(value, ',')) {
        const int col = column(g);
        if (col >= 0) {
          p.query.group_by.push_back(col);
        }
      }
    } else if (key == 'a') {
      static constexpr std::string_view kAggNames[] = {
          "count", "sum", "min", "max", "mean"};
      for (std::string_view a : Split(value, ',')) {
        const auto f = Split(a, ':');
        const int col = column(f[0]);
        if (col < 0 || f.size() != 2) {
          continue;
        }
        for (int op = 0; op < 5; ++op) {
          if (kAggNames[op] == f[1]) {
            p.query.aggregates.push_back(
                {col, static_cast<data::AggregateOp>(op)});
          }
        }
      }
    } else if (key == 's') {
      for (std::string_view k : Split(value, ',')) {
        const auto f = Split(k, ':');
        const int col = column(f[0]);
        if (col >= 0 && f.size() == 2) {
          p.query.sort.push_back({col,
                                  f[1] == "d"
                                      ? data::SortDirection::kDescending
                                      : data::SortDirection::kAscending});
        }
      }
    } else if (key == 'f') {
      const auto f = Split(value, ':');
      if (f.size() != 6) {
        continue;
      }
      data::Filter filter;
      filter.column = column(f[0]);
      if (filter.column < 0) {
        continue;
      }
      bool known = false;
      for (const OpName &o : kOps) {
        if (o.name == f[1]) {
          filter.op = o.op;
          known = true;
        }
      }
      if (!known) {
        continue;
      }
      filter.a = ParseNumber(f[2]);
      filter.b = ParseNumber(f[3]);
      filter.text = Unescape(f[4]);
      if (!f[5].empty()) {
        for (std::string_view v : Split(f[5], '|')) {
          std::int64_t x = 0;
          std::from_chars(v.data(), v.data() + v.size(), x);
          filter.set.push_back(x);
        }
      }
      p.query.filters.push_back(std::move(filter));
    } else if (key == 'c') {
      for (std::string_view c : Split(value, ',')) {
        const auto f = Split(c, ':');
        const int col = column(f[0]);
        if (col < 0 || f.size() != 3) {
          continue;
        }
        GridColumn gc;
        gc.column = col;
        gc.width = static_cast<float>(ParseNumber(f[1]));
        gc.visible = f[2].find('h') == std::string_view::npos;
        gc.pinned = f[2].find('p') != std::string_view::npos;
        p.columns.push_back(gc);
      }
    }
  }
  return p;
}

namespace {

/// The width of a row that fits its content last frame.
struct FitRowState {
  float width = 0.0F;
};

/// Width of a Button() with `text`.
auto ButtonWidth(Context &ctx, std::string_view text) -> float {
  const Theme &t = ctx.theme();
  return std::ceil(
             ctx.text().Measure(text, t.fonts.sans, t.metrics.font_size)) +
         t.metrics.padding_x * 3.0F;
}

/// Width of a BeginMenuButton() with `text` (and its chevron).
auto MenuButtonWidth(Context &ctx, std::string_view text) -> float {
  const Theme &t = ctx.theme();
  return std::ceil(
             ctx.text().Measure(text, t.fonts.sans, t.metrics.font_size)) +
         t.metrics.padding_x * 2.0F + 27.0F;
}

}  // namespace

auto PresetBar(Context &ctx, std::string_view label,
               std::span<const ViewPreset> presets, int &active)
    -> PresetBarResult {
  const Theme &t = ctx.theme();
  PresetBarResult result;
  const Id id = ctx.GetId(label);
  constexpr float kGap = 6.0F;
  constexpr float kChipPadding = 10.0F;
  constexpr float kDot = 6.0F;

  // Which chips fit the width the row had last frame: all of them with
  // Save and Share, else the chosen one and those that fit before a
  // "More" menu holding the rest, Save and Share.
  auto &state = ctx.State<FitRowState>(id);
  if (const std::optional<Rect> last = ctx.LastRect(id)) {
    state.width = last->Width();
  }
  const float available = state.width > 0.0F ? state.width : ctx.viewport().x;
  std::vector<float> widths;
  std::vector<bool> keep;
  float all =
      ButtonWidth(ctx, "Save view") + ButtonWidth(ctx, "Share") + 2 * kGap;
  for (std::size_t i = 0; i < presets.size(); ++i) {
    const bool on = active == static_cast<int>(i);
    const float w =
        std::ceil(ctx.text().Measure(presets[i].name,
                                     on ? t.fonts.bold : t.fonts.sans,
                                     t.metrics.font_size)) +
        2 * kChipPadding + (presets[i].curated ? kDot + 5.0F : 0.0F) +
        2 * t.metrics.border_width;
    widths.push_back(w);
    keep.push_back(on);
    all += w + kGap;
  }
  const bool crowded = all > available;
  const std::vector<bool> shown =
      crowded ? FitSegments(widths,
                            keep,
                            0.0F,
                            kGap,
                            available - MenuButtonWidth(ctx, "More") - kGap)
              : std::vector<bool>(presets.size(), true);

  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.gap = kGap;
  row.layout.align = Align::kCenter;
  row.layout.width = SizeSpec::Grow();
  row.flags = kNodeClip;
  ctx.Open(row);
  ctx.PushId(id);
  for (std::size_t i = 0; i < presets.size(); ++i) {
    if (!shown[i]) {
      continue;
    }
    const ViewPreset &p = presets[i];
    const Id chip_id = ctx.GetId(static_cast<std::uint64_t>(i));
    const Interaction it = ctx.Interact(chip_id);
    if (it.clicked ||
        (ctx.HasFocus(chip_id) &&
         (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace)))) {
      if (active != static_cast<int>(i)) {
        active = static_cast<int>(i);
        result.changed = true;
      }
    }
    const bool on = active == static_cast<int>(i);
    NodeDesc chip;
    chip.id = chip_id;
    chip.layout.mode = LayoutMode::kRow;
    chip.layout.height = SizeSpec::Fixed(t.metrics.control_height - 2);
    chip.layout.padding = Padding::Xy(kChipPadding, 0);
    chip.layout.gap = 5;
    chip.layout.align = Align::kCenter;
    chip.style.radii =
        gfx::CornerRadii::All((t.metrics.control_height - 2) * 0.5F);
    chip.style.fill = on ? t.colors.primary.WithAlpha(0.22F)
                         : detail::InteractiveFill(t, t.colors.surface, it);
    chip.style.border = on ? t.colors.primary : t.colors.border;
    chip.style.border_width = t.metrics.border_width;
    chip.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
    ctx.Open(chip);
    if (p.curated) {
      // Curated presets carry a small dot.
      NodeDesc dot;
      dot.layout.width = SizeSpec::Fixed(kDot);
      dot.layout.height = SizeSpec::Fixed(kDot);
      dot.style.fill = t.colors.accent;
      dot.style.radii = gfx::CornerRadii::All(kDot * 0.5F);
      ctx.Open(dot);
      ctx.Close();
    }
    TextSpec text;
    text.text = p.name;
    text.font = on ? t.fonts.bold : t.fonts.sans;
    text.size = t.metrics.font_size;
    text.color = on ? t.colors.text : t.colors.text_muted;
    ctx.Text(text);
    ctx.Close();
    if (!p.description.empty()) {
      Tooltip(ctx, chip_id, p.description);
    }
  }
  Spacer(ctx);
  if (!crowded) {
    if (Button(ctx, "Save view", ButtonKind::kFlat)) {
      result.save = true;
    }
    if (Button(ctx, "Share", ButtonKind::kFlat)) {
      result.share = true;
    }
  } else if (BeginMenuButton(ctx, "More##more", ButtonKind::kFlat)) {
    bool any = false;
    for (std::size_t i = 0; i < presets.size(); ++i) {
      if (!shown[i]) {
        any = true;
        if (MenuItem(
                ctx,
                MenuItemSpec{presets[i].name, {}, presets[i].description})) {
          active = static_cast<int>(i);
          result.changed = true;
        }
      }
    }
    if (any) {
      MenuSeparator(ctx);
    }
    if (MenuItem(ctx,
                 MenuItemSpec{"Save view", {}, "Keep this filter and sort"})) {
      result.save = true;
    }
    if (MenuItem(ctx,
                 MenuItemSpec{"Share view", {}, "Copy it as text to paste"})) {
      result.share = true;
    }
    EndMenuButton(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

auto FitSegments(std::span<const float> widths, const std::vector<bool> &keep,
                 float frame, float gap, float available) -> std::vector<bool> {
  const std::size_t n = widths.size();
  std::vector<bool> shown(n, true);
  const auto kept = [&](std::size_t i) {
    return i < keep.size() && keep[i];
  };
  const auto total = [&] {
    float w = frame;
    int count = 0;
    for (std::size_t i = 0; i < n; ++i) {
      if (shown[i]) {
        w += widths[i];
        ++count;
      }
    }
    return w + gap * static_cast<float>(std::max(0, count - 1));
  };
  for (std::size_t k = n; k > 0 && total() > available; --k) {
    if (!kept(k - 1)) {
      shown[k - 1] = false;
    }
  }
  return shown;
}

namespace {

/// The width of the segment row last frame.
struct PresetSegmentsState {
  float width = 0.0F;
};

}  // namespace

auto PresetSegments(Context &ctx, std::string_view label,
                    std::span<const ViewPreset> presets, int &active,
                    const PresetSegmentsOptions &opt) -> PresetBarResult {
  PresetBarResult result;
  const Theme &t = ctx.theme();
  const int n = static_cast<int>(presets.size());
  const auto count_of = [&](int i) {
    return i < static_cast<int>(opt.counts.size())
               ? opt.counts[static_cast<std::size_t>(i)]
               : -1;
  };
  // Which presets may be segments: the chosen one always, then those with
  // rows (or every one, without counts), in order, up to the limit.
  std::vector<int> candidates;
  std::vector<int> empty;
  int room = std::max(1, opt.max_segments) - (active >= 0 ? 1 : 0);
  for (int i = 0; i < n; ++i) {
    const bool chosen = i == active;
    const bool worth = !opt.hide_empty || count_of(i) != 0;
    if (chosen || (worth && room > 0)) {
      candidates.push_back(i);
      room -= chosen ? 0 : 1;
    } else {
      empty.push_back(i);
    }
  }
  // Then those that fit the width the row had last frame.
  const Id id = ctx.GetId(label);
  auto &state = ctx.State<PresetSegmentsState>(id);
  if (const std::optional<Rect> last = ctx.LastRect(id)) {
    state.width = last->Width();
  }
  const float available = state.width > 0.0F ? state.width : ctx.viewport().x;
  constexpr float kRowGap = 8.0F;
  const float views_width = std::ceil(ctx.text().Measure(
                                "Views", t.fonts.sans, t.metrics.font_size)) +
                            t.metrics.padding_x * 2.0F + 27.0F;
  std::vector<float> widths;
  std::vector<bool> keep;
  for (const int i : candidates) {
    const ViewPreset &p = presets[static_cast<std::size_t>(i)];
    widths.push_back(
        SegmentWidth(ctx, Segment{p.name, count_of(i), {}}, i == active));
    keep.push_back(i == active);
  }
  if (active < 0) {
    widths.push_back(SegmentWidth(ctx, Segment{"Your view", -1, {}}, true));
    keep.push_back(true);
  }
  const float frame = SegmentedControlWidth(ctx, {}, -1);
  const std::vector<bool> fits = FitSegments(
      widths, keep, frame, 2.0F, available - views_width - kRowGap - 2.0F);
  std::vector<int> shown;
  std::vector<int> crowded;
  for (std::size_t k = 0; k < candidates.size(); ++k) {
    (fits[k] ? shown : crowded).push_back(candidates[k]);
  }
  std::vector<Segment> segments;
  for (const int i : shown) {
    const ViewPreset &p = presets[static_cast<std::size_t>(i)];
    segments.push_back({p.name, count_of(i), p.description});
  }
  int selected = -1;
  for (std::size_t k = 0; k < shown.size(); ++k) {
    if (shown[k] == active) {
      selected = static_cast<int>(k);
    }
  }
  if (active < 0) {
    segments.push_back({"Your view", -1, "Changed from the saved views"});
    selected = static_cast<int>(segments.size()) - 1;
  }
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.width = SizeSpec::Grow();
  row.layout.gap = kRowGap;
  row.layout.align = Align::kCenter;
  row.flags = kNodeClip;
  ctx.Open(row);
  ctx.PushId(label);
  const int before = selected;
  if (SegmentedControl(ctx, "##segments", selected, segments) &&
      selected != before && selected >= 0 &&
      selected < static_cast<int>(shown.size())) {
    active = shown[static_cast<std::size_t>(selected)];
    result.changed = true;
  }
  const auto menu_views = [&](std::span<const int> views,
                              std::string_view heading) {
    if (views.empty()) {
      return;
    }
    MenuHeading(ctx, heading);
    for (const int i : views) {
      const ViewPreset &p = presets[static_cast<std::size_t>(i)];
      const int c = count_of(i);
      const std::string text =
          c >= 0 ? p.name + " (" + std::to_string(c) + ")" : p.name;
      if (MenuItem(ctx, MenuItemSpec{text, {}, p.description, true})) {
        active = i;
        result.changed = true;
      }
    }
    MenuSeparator(ctx);
  };
  if (BeginMenuButton(ctx, "Views##more", ButtonKind::kFlat)) {
    menu_views(crowded, "More views");
    menu_views(empty, opt.hide_empty ? "Nothing in them now" : "More views");
    if (MenuItem(ctx,
                 MenuItemSpec{"Save view", {}, "Keep this filter and sort"})) {
      result.save = true;
    }
    if (MenuItem(ctx,
                 MenuItemSpec{"Share view", {}, "Copy it as text to paste"})) {
      result.share = true;
    }
    EndMenuButton(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

}  // namespace gloomy::ui
