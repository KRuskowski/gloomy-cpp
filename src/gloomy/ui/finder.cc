#include "gloomy/ui/finder.h"

#include <algorithm>
#include <cmath>

#include "gloomy/core/utf8.h"
#include "gloomy/data/fuzzy.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

constexpr int kPerGroup = 6;
constexpr int kSingleGroup = 40;

// Highlight ranges for fuzzy-matched character positions.
auto PositionsToRanges(std::string_view text,
                       const std::vector<std::uint32_t> &positions)
    -> std::vector<data::MatchRange> {
  std::vector<data::MatchRange> ranges;
  for (const std::uint32_t p : positions) {
    std::size_t end = p;
    DecodeUtf8(text, end);
    if (!ranges.empty() && ranges.back().end == p) {
      ranges.back().end = static_cast<std::uint32_t>(end);
    } else {
      ranges.push_back({p, static_cast<std::uint32_t>(end)});
    }
  }
  return ranges;
}

}  // namespace

// CommandProvider

void CommandProvider::SetQuery(std::string_view query) {
  ranked_.clear();
  for (std::size_t i = 0; i < commands_.size(); ++i) {
    if (query.empty()) {
      ranked_.push_back({i, 0, {}});
      continue;
    }
    if (auto m = data::FuzzyMatchText(query, commands_[i].title)) {
      ranked_.push_back({i, m->score, std::move(m->positions)});
    }
  }
  std::stable_sort(
      ranked_.begin(), ranked_.end(), [](const Ranked &a, const Ranked &b) {
        return a.score > b.score;
      });
}

void CommandProvider::Collect(int limit, std::vector<SearchHit> &out) {
  for (std::size_t i = 0; i < ranked_.size() && static_cast<int>(i) < limit;
       ++i) {
    const Command &c = commands_[ranked_[i].index];
    SearchHit hit;
    hit.label = c.title;
    hit.detail = c.shortcut;
    hit.highlights = PositionsToRanges(c.title, ranked_[i].positions);
    hit.go = c.run;
    out.push_back(std::move(hit));
  }
}

// TableProvider

TableProvider::TableProvider(std::string name,
                             std::shared_ptr<const data::TableSource> source,
                             int label_column, std::vector<int> context_columns,
                             std::function<void(data::RowIndex)> on_select)
    : name_(std::move(name)),
      view_(std::move(source)),
      label_column_(label_column),
      context_columns_(std::move(context_columns)),
      on_select_(std::move(on_select)) {}

void TableProvider::SetQuery(std::string_view query) {
  empty_query_ = data::ParseTerms(query).empty();
  data::Query q;
  q.search = std::string(query);
  if (empty_query_) {
    // Nothing to show; keep the view cheap.
    data::Filter none;
    none.column = label_column_;
    none.op = data::FilterOp::kTextEquals;
    none.text = "\x01";
    q.filters.push_back(std::move(none));
  }
  view_.SetQuery(q);
}

auto TableProvider::count() const -> std::size_t {
  return empty_query_ ? 0 : view_.rows().size();
}

void TableProvider::Collect(int limit, std::vector<SearchHit> &out) {
  if (empty_query_ || view_.view().query != view_.query()) {
    return;
  }
  const data::TableSource &src = view_.source();
  const auto rows = view_.rows();
  std::string cell;
  for (std::size_t i = 0; i < rows.size() && static_cast<int>(i) < limit; ++i) {
    const data::RowIndex r = rows[i];
    if (r >= src.RowCount()) {
      // The source shrank since the query; the next update runs it again.
      continue;
    }
    SearchHit hit;
    src.FormatCell(r, label_column_, hit.label);
    for (const int c : context_columns_) {
      src.FormatCell(r, c, cell);
      if (!hit.context.empty()) {
        hit.context += " \xC2\xB7 ";
      }
      hit.context += cell;
    }
    data::FindMatches(hit.label, view_.terms(), hit.highlights);
    hit.go = [select = on_select_, r] {
      select(r);
    };
    out.push_back(std::move(hit));
  }
}

// DocumentProvider

DocumentProvider::DocumentProvider(
    std::string name, const data::Document &doc,
    std::function<void(const data::DocMatch &)> on_select)
    : name_(std::move(name)), doc_(doc), on_select_(std::move(on_select)) {}

void DocumentProvider::SetQuery(std::string_view query) {
  terms_ = data::ParseTerms(query);
  Match();
}

void DocumentProvider::Match() {
  matches_.clear();
  matched_version_ = doc_.version();
  const std::vector<std::string> &terms = terms_;
  if (terms.empty()) {
    return;
  }
  const data::SearchCorpus &corpus = doc_.corpus();
  std::vector<data::MatchRange> ranges;
  for (std::uint32_t b = 0; b < corpus.size(); ++b) {
    if (!corpus.Matches(b, terms)) {
      continue;
    }
    ranges.clear();
    data::FindMatches(doc_.blocks()[b].text, terms, ranges);
    if (!ranges.empty()) {
      matches_.push_back({b, ranges.front()});
    }
  }
}

void DocumentProvider::Collect(int limit, std::vector<SearchHit> &out) {
  if (doc_.version() != matched_version_) {
    // The document changed since the query: its blocks are others.
    Match();
  }
  const auto blocks = doc_.blocks();
  for (std::size_t i = 0; i < matches_.size() && static_cast<int>(i) < limit;
       ++i) {
    const data::DocMatch &m = matches_[i];
    const std::string &text = blocks[m.block].text;
    // A snippet around the match.
    std::size_t begin = m.range.begin > 40 ? m.range.begin - 40 : 0;
    while (begin > 0 &&
           (static_cast<unsigned char>(text[begin]) & 0xC0U) == 0x80U) {
      --begin;
    }
    const std::size_t end = std::min(text.size(), begin + 120);
    SearchHit hit;
    if (begin > 0) {
      hit.label = "\xE2\x80\xA6";
    }
    hit.label += text.substr(begin, end - begin);
    data::FindMatches(hit.label, terms_, hit.highlights);
    // The section the block belongs to.
    for (std::uint32_t b = m.block + 1; b-- > 0;) {
      if (blocks[b].kind == data::BlockKind::kHeading) {
        hit.context = blocks[b].text;
        break;
      }
    }
    hit.go = [select = on_select_, m] {
      select(m);
    };
    out.push_back(std::move(hit));
  }
}

// Finder

auto Finder(Context &ctx, std::string_view label,
            std::span<SearchProvider *const> providers, FinderState &state,
            std::string_view placeholder) -> bool {
  if (!state.open) {
    return false;
  }
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  ctx.PushId(id);
  const Id field = SearchFieldId(ctx, "##q");
  const Id panel_id = ctx.GetId("##panel");

  // Collect results first so the keys can move through them.
  if (state.query != state.searched) {
    for (SearchProvider *p : providers) {
      p->SetQuery(state.query);
    }
    state.searched = state.query;
    state.selected = 0;
  }
  const int per_group = providers.size() == 1 ? kSingleGroup : kPerGroup;

  struct Group {
    SearchProvider *provider;
    std::size_t first;
    std::size_t count;
  };

  std::vector<SearchHit> hits;
  std::vector<Group> groups;
  for (SearchProvider *p : providers) {
    p->Update(ctx.time());
    const std::size_t first = hits.size();
    p->Collect(per_group, hits);
    if (hits.size() > first || p->busy()) {
      groups.push_back({p, first, hits.size() - first});
    }
  }
  const int total = static_cast<int>(hits.size());
  state.selected = std::clamp(state.selected, 0, std::max(0, total - 1));

  bool activated = false;
  bool close = false;
  if (ctx.ConsumeKey(Key::kDown)) {
    state.selected = std::min(total - 1, state.selected + 1);
  }
  if (ctx.ConsumeKey(Key::kUp)) {
    state.selected = std::max(0, state.selected - 1);
  }
  int activate = -1;
  if (ctx.ConsumeKey(Key::kEnter) && total > 0) {
    activate = state.selected;
  }
  if (state.query.empty() && ctx.ConsumeKey(Key::kEscape)) {
    close = true;
  }
  if (ctx.Interact(HashCombine(id, 0x6C61796572ULL)).clicked) {
    // A click on the dimmed background.
    close = true;
  }

  ctx.BeginLayer(LayerKind::kModal, id);
  const Vec2 vp = ctx.viewport();
  NodeDesc panel;
  panel.id = panel_id;
  panel.layout.mode = LayoutMode::kColumn;
  panel.layout.width = SizeSpec::Fixed(std::min(680.0F, vp.x - 40));
  panel.layout.height = SizeSpec::Fit(0, vp.y * 0.75F);
  panel.layout.padding = Padding::All(10);
  panel.layout.gap = 6;
  panel.layout.align = Align::kStretch;
  panel.style.fill = t.colors.surface_raised;
  panel.style.border = t.colors.border;
  panel.style.border_width = t.metrics.border_width;
  panel.style.radii = gfx::CornerRadii::All(t.metrics.radius + 4);
  panel.style.shadow = t.colors.shadow;
  panel.style.shadow_blur = 18;
  panel.style.shadow_offset = {0, 6};
  panel.flags = kNodeInteractive;
  ctx.Open(panel);
  if (ctx.focused() != field) {
    ctx.SetFocus(field, false);
  }
  SearchFieldOptions opt;
  opt.placeholder = placeholder;
  std::size_t found = 0;
  bool busy = false;
  for (SearchProvider *p : providers) {
    found += p->count();
    busy = busy || p->busy();
  }
  const std::string status =
      state.query.empty() ? std::string() : std::to_string(found) + " results";
  opt.status = status;
  opt.busy = busy;
  SearchField(ctx, "##q", state.query, opt);

  LayoutSpec list;
  list.height = SizeSpec::Fit(0, vp.y * 0.75F - 60);
  list.gap = 1;
  list.align = Align::kStretch;
  BeginScroll(ctx, "##results", list);
  int index = 0;
  for (const Group &g : groups) {
    std::string header(g.provider->name());
    header += "  " + std::to_string(g.provider->count());
    if (g.provider->busy()) {
      header += " \xE2\x80\xA6";
    }
    LabelOptions head;
    head.muted = true;
    head.bold = true;
    head.size = t.metrics.small_font_size;
    LayoutSpec head_row;
    head_row.padding = {6, 6, 6, 2};
    BeginRow(ctx, head_row);
    Label(ctx, header, head);
    End(ctx);
    for (std::size_t i = g.first; i < g.first + g.count; ++i, ++index) {
      SearchHit &hit = hits[i];
      const Id row_id = ctx.GetId(static_cast<std::uint64_t>(index));
      const Interaction it = ctx.Interact(row_id);
      if (it.hovered && ctx.mouse_down()) {
        state.selected = index;
      }
      if (it.clicked) {
        activate = index;
      }
      NodeDesc row;
      row.id = row_id;
      row.layout.mode = LayoutMode::kRow;
      row.layout.height = SizeSpec::Fixed(t.metrics.row_height + 6);
      row.layout.padding = Padding::Xy(8, 0);
      row.layout.gap = 10;
      row.layout.align = Align::kCenter;
      row.style.radii = gfx::CornerRadii::All(t.metrics.radius);
      if (index == state.selected) {
        row.style.fill = t.colors.selection;
      } else if (it.hovered) {
        row.style.fill = t.colors.hover;
      }
      row.flags = kNodeInteractive;
      ctx.Open(row);
      TextSpec text;
      text.text = hit.label;
      text.font = t.fonts.sans;
      text.size = t.metrics.font_size;
      text.color = t.colors.text;
      LayoutSpec text_layout;
      text_layout.width = SizeSpec::Fit();
      const int label_node = ctx.Text(text, text_layout);
      if (!hit.highlights.empty()) {
        const std::string shown = hit.label;
        const std::vector<data::MatchRange> ranges = hit.highlights;
        const text::FontId font = t.fonts.sans;
        const float size = t.metrics.font_size;
        const Color color = t.colors.highlight;
        ctx.AddPainter(
            label_node,
            [=](PaintContext &pc, const Rect &r) {
              const text::ShapedText &s = pc.text.Shape(shown, font, size);
              for (const data::MatchRange &m : ranges) {
                pc.list.FillRect({r.x0 + s.CaretX(m.begin),
                                  r.y0 + 2,
                                  r.x0 + s.CaretX(m.end),
                                  r.y1 - 2},
                                 color,
                                 gfx::CornerRadii::All(2));
              }
            },
            true);
      }
      if (!hit.context.empty()) {
        LabelOptions ctx_opt;
        ctx_opt.muted = true;
        ctx_opt.size = t.metrics.small_font_size + 1;
        ctx_opt.width = SizeSpec::Grow();
        Label(ctx, hit.context, ctx_opt);
      } else {
        Spacer(ctx);
      }
      if (!hit.detail.empty()) {
        LabelOptions detail;
        detail.muted = true;
        detail.mono = true;
        detail.size = t.metrics.small_font_size;
        Label(ctx, hit.detail, detail);
      }
      ctx.Close();
    }
  }
  if (groups.empty() && !state.query.empty()) {
    LabelOptions none;
    none.muted = true;
    LayoutSpec none_row;
    none_row.padding = Padding::All(8);
    BeginRow(ctx, none_row);
    Label(ctx, busy ? "Searching\xE2\x80\xA6" : "Nothing found", none);
    End(ctx);
  }
  EndScroll(ctx);
  ctx.Close();
  ctx.EndLayer();
  ctx.PopId();

  if (activate >= 0 && activate < total) {
    if (hits[static_cast<std::size_t>(activate)].go) {
      hits[static_cast<std::size_t>(activate)].go();
    }
    activated = true;
    close = true;
  }
  if (close) {
    state.open = false;
    state.query.clear();
    state.searched = "\x01";
    ctx.SetFocus(kNoId);
  }
  return activated;
}

}  // namespace gloomy::ui
