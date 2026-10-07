#include "gloomy/ui/document_view.h"

#include <algorithm>
#include <cmath>

#include "gloomy/data/search.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

using data::BlockKind;
using data::DocBlock;

constexpr float kPad = 16.0F;
constexpr double kLineScroll = 40.0;

// Fenwick (binary indexed) tree over block heights: prefix sums and
// position lookup in O(log n), so scroll offsets map to blocks quickly
// even for long documents with estimated heights.
void FenwickInit(std::vector<double> &tree, const std::vector<float> &values) {
  const std::size_t n = values.size();
  tree.assign(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    tree[i + 1] += values[i];
    const std::size_t parent = (i + 1) + ((i + 1) & (~(i + 1) + 1));
    if (parent <= n) {
      tree[parent] += tree[i + 1];
    }
  }
}

void FenwickAdd(std::vector<double> &tree, std::size_t i, double delta) {
  for (++i; i < tree.size(); i += i & (~i + 1)) {
    tree[i] += delta;
  }
}

// Sum of the first `count` values.
auto FenwickPrefix(const std::vector<double> &tree, std::size_t count)
    -> double {
  double sum = 0.0;
  for (std::size_t i = count; i > 0; i -= i & (~i + 1)) {
    sum += tree[i];
  }
  return sum;
}

// Index of the value containing offset `y` (clamped to the last one).
auto FenwickFind(const std::vector<double> &tree, double y) -> std::size_t {
  const std::size_t n = tree.size() - 1;
  if (n == 0) {
    return 0;
  }
  std::size_t pos = 0;
  std::size_t step = 1;
  while (step * 2 <= n) {
    step *= 2;
  }
  for (; step > 0; step /= 2) {
    if (pos + step <= n && tree[pos + step] <= y) {
      pos += step;
      y -= tree[pos];
    }
  }
  return std::min(pos, n - 1);
}

// Vertical space around a block, and its left indent.
struct BlockBox {
  float top = 0.0F;
  float bottom = 0.0F;
  float indent = 0.0F;
};

auto BoxOf(const DocBlock &b) -> BlockBox {
  switch (b.kind) {
    case BlockKind::kHeading:
      return {b.level == 1 ? 6.0F : 14.0F, b.level == 1 ? 14.0F : 6.0F, 0.0F};
    case BlockKind::kListItem:
      return {0.0F, 4.0F, 18.0F + 16.0F * static_cast<float>(b.level)};
    case BlockKind::kQuote:
      return {2.0F, 10.0F, 14.0F};
    case BlockKind::kCode:
      return {8.0F, 18.0F, 10.0F};
    case BlockKind::kRule:
      return {8.0F, 8.0F, 0.0F};
    case BlockKind::kParagraph:
      break;
  }
  return {0.0F, 10.0F, 0.0F};
}

auto BodySize(const Theme &t) -> float {
  return t.metrics.font_size + 1.0F;
}

// Text runs of a block from its kind, spans and the theme.
auto RunsOf(const DocBlock &b, const Theme &t) -> std::vector<text::TextRun> {
  const Palette &c = t.colors;
  text::TextRun base;
  base.font = t.fonts.sans;
  base.size = BodySize(t);
  base.color = c.text;
  switch (b.kind) {
    case BlockKind::kHeading:
      base.font = t.fonts.bold;
      base.size = b.level == 1   ? t.metrics.heading_font_size + 6
                  : b.level == 2 ? t.metrics.heading_font_size + 2
                                 : t.metrics.font_size + 2;
      break;
    case BlockKind::kQuote:
      base.font = t.fonts.italic;
      base.color = c.text_muted;
      break;
    case BlockKind::kCode:
      base.font = t.fonts.mono;
      base.size = t.metrics.font_size;
      break;
    default:
      break;
  }
  std::vector<text::TextRun> runs;
  const auto len = static_cast<std::uint32_t>(b.text.size());
  std::uint32_t at = 0;
  auto plain = [&](std::uint32_t end) {
    if (end > at) {
      text::TextRun r = base;
      r.begin = at;
      r.end = end;
      runs.push_back(r);
    }
  };
  for (const data::DocSpan &s : b.spans) {
    plain(s.begin);
    text::TextRun r = base;
    r.begin = s.begin;
    r.end = s.end;
    if ((s.style & data::kSpanBold) != 0) {
      r.font = t.fonts.bold;
    } else if ((s.style & data::kSpanItalic) != 0) {
      r.font = t.fonts.italic;
    }
    if ((s.style & data::kSpanMono) != 0) {
      r.font = t.fonts.mono;
      r.background = c.surface_alt;
    }
    if ((s.style & data::kSpanLink) != 0) {
      r.color = c.accent;
      r.underline = true;
    }
    if ((s.style & data::kSpanHighlight) != 0) {
      r.background = c.highlight;
    }
    if (s.color != 0) {
      r.color = Color::Rgba(s.color);
    }
    runs.push_back(r);
    at = s.end;
  }
  plain(len);
  if (runs.empty()) {
    text::TextRun r = base;
    runs.push_back(r);
  }
  return runs;
}

// Rough height before a block has been laid out.
auto Estimate(const DocBlock &b, float width, const Theme &t) -> float {
  const BlockBox box = BoxOf(b);
  if (b.kind == BlockKind::kRule) {
    return box.top + 1.0F + box.bottom;
  }
  const float line = std::ceil(BodySize(t) * 1.25F);
  const float chars_per_line =
      std::max(10.0F, (width - box.indent) / (BodySize(t) * 0.5F));
  const float lines =
      std::ceil(static_cast<float>(b.text.size() + 1) / chars_per_line);
  return box.top + lines * line + box.bottom;
}

void Reset(const data::Document &doc, DocumentViewState &st, float width,
           const Theme &t) {
  const auto blocks = doc.blocks();
  st.heights.resize(blocks.size());
  st.exact.assign(blocks.size(), 0);
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    st.heights[i] = Estimate(blocks[i], width, t);
  }
  FenwickInit(st.fenwick, st.heights);
  st.layouts.clear();
  st.runs.clear();
  st.width = width;
  st.doc_version = doc.version();
}

// Lays out block `b` if needed and fixes its height.
auto EnsureLayout(Context &ctx, const data::Document &doc,
                  DocumentViewState &st, std::uint32_t b)
    -> const text::RichLayout & {
  auto it = st.layouts.find(b);
  if (it != st.layouts.end()) {
    return it->second;
  }
  const Theme &t = ctx.theme();
  const DocBlock &block = doc.blocks()[b];
  const BlockBox box = BoxOf(block);
  std::vector<text::TextRun> &runs = st.runs[b];
  runs = RunsOf(block, t);
  text::RichLayout layout;
  if (block.kind != BlockKind::kRule) {
    const float pad = block.kind == BlockKind::kCode ? 2 * box.indent : 0.0F;
    layout = text::LayoutRich(
        ctx.text(), block.text, runs, st.width - box.indent - pad, 2.0F);
  }
  float h = box.top + box.bottom +
            (block.kind == BlockKind::kRule ? 1.0F : layout.height);
  if (block.kind == BlockKind::kCode) {
    h += 2 * box.indent;
  }
  FenwickAdd(st.fenwick, b, h - st.heights[b]);
  st.heights[b] = h;
  st.exact[b] = 1;
  return st.layouts.emplace(b, std::move(layout)).first->second;
}

// Top of block `b`'s text, relative to the block's top.
auto TextTop(const DocBlock &b) -> float {
  const BlockBox box = BoxOf(b);
  return box.top + (b.kind == BlockKind::kCode ? box.indent : 0.0F);
}

void PaintDocument(PaintContext &pc, const Rect &r, const data::Document &doc,
                   const DocumentViewState &st) {
  const Palette &c = pc.theme.colors;
  gfx::DrawList &list = pc.list;
  list.FillRect(r, c.surface);
  if (st.fenwick.size() <= 1) {
    return;
  }
  const auto blocks = doc.blocks();
  const double viewport = r.Height();
  std::size_t b = FenwickFind(st.fenwick, st.scroll);
  for (; b < blocks.size(); ++b) {
    const double top = FenwickPrefix(st.fenwick, b) - st.scroll;
    if (top > viewport) {
      break;
    }
    auto it = st.layouts.find(static_cast<std::uint32_t>(b));
    if (it == st.layouts.end()) {
      continue;
    }
    const DocBlock &block = blocks[b];
    const BlockBox box = BoxOf(block);
    const float y0 = r.y0 + static_cast<float>(top);
    const float x0 = r.x0 + kPad + box.indent;
    const text::RichLayout &layout = it->second;
    const float text_y = y0 + TextTop(block);
    switch (block.kind) {
      case BlockKind::kRule:
        list.FillRect(
            {r.x0 + kPad, y0 + box.top, r.x1 - kPad, y0 + box.top + 1},
            c.divider);
        continue;
      case BlockKind::kQuote:
        list.FillRect(
            {r.x0 + kPad, text_y, r.x0 + kPad + 3, text_y + layout.height},
            c.accent.WithAlpha(0.6F),
            gfx::CornerRadii::All(1.5F));
        break;
      case BlockKind::kCode:
        list.FillRect({r.x0 + kPad,
                       y0 + box.top,
                       r.x1 - kPad,
                       y0 + st.heights[b] - box.bottom},
                      c.surface_alt,
                      gfx::CornerRadii::All(4));
        break;
      case BlockKind::kListItem:
        if (!layout.lines.empty()) {
          const float cy =
              text_y + layout.lines[0].baseline - BodySize(pc.theme) * 0.33F;
          const float cx = x0 - 9;
          if (block.text.empty() || block.text[0] < '0' ||
              block.text[0] > '9') {
            if (block.level == 0) {
              pc.list.FillCircle({cx, cy}, 2.5F, c.text_muted);
            } else {
              pc.list.StrokeCircle({cx, cy}, 2.5F, c.text_muted, 1.0F);
            }
          }
        }
        break;
      case BlockKind::kHeading:
        if (block.level == 1) {
          list.FillRect({r.x0 + kPad,
                         text_y + layout.height + 5,
                         r.x1 - kPad,
                         text_y + layout.height + 6},
                        c.divider);
        }
        break;
      default:
        break;
    }
    const float lx = block.kind == BlockKind::kCode ? x0 + box.indent : x0;
    // Search highlights; the current match stronger and outlined.
    for (std::size_t m = 0; m < st.matches.size(); ++m) {
      const data::DocMatch &match = st.matches[m];
      if (match.block != b) {
        continue;
      }
      const bool current = static_cast<int>(m) == st.current;
      for (const Rect &hr :
           layout.RangeRects(match.range.begin, match.range.end)) {
        const Rect box_r = hr.Offset({lx, text_y});
        list.FillRect(box_r,
                      current ? c.warning.WithAlpha(0.55F) : c.highlight,
                      gfx::CornerRadii::All(2));
        if (current) {
          list.StrokeRect(
              box_r.Inset(-1), c.warning, 1.0F, gfx::CornerRadii::All(3));
        }
      }
    }
    const auto runs = st.runs.find(static_cast<std::uint32_t>(b));
    if (runs != st.runs.end()) {
      text::DrawRich(pc.text, list, layout, runs->second, {lx, text_y});
    }
  }
}

}  // namespace

auto TopBlock(const DocumentViewState &state) -> std::uint32_t {
  if (state.fenwick.size() <= 1) {
    return 0;
  }
  return static_cast<std::uint32_t>(FenwickFind(state.fenwick, state.scroll));
}

void GoToMatch(DocumentViewState &state, bool backwards) {
  if (state.matches.empty()) {
    return;
  }
  const int n = static_cast<int>(state.matches.size());
  state.current = state.current < 0
                      ? (backwards ? n - 1 : 0)
                      : (state.current + (backwards ? n - 1 : 1)) % n;
  state.scroll_to_match = true;
}

void ScrollToBlock(DocumentViewState &state, std::uint32_t block) {
  state.scroll_to_block = block;
}

auto MatchStatus(const DocumentViewState &state) -> std::string {
  if (state.searched.empty()) {
    return {};
  }
  if (state.matches.empty()) {
    return "No matches";
  }
  return std::to_string(state.current + 1) + " of " +
         std::to_string(state.matches.size());
}

auto DocumentView(Context &ctx, std::string_view label,
                  const data::Document &doc, DocumentViewState &st,
                  LayoutSpec layout) -> DocumentResult {
  const Theme &t = ctx.theme();
  DocumentResult result;
  const Id id = ctx.GetId(label);
  const Id bar_id = HashCombine(id, 0x76626172ULL);
  const Interaction it = ctx.Interact(id);
  const Rect rect = it.rect;
  const float width = std::max(100.0F, rect.Width() - 2 * kPad);
  if (st.doc_version != doc.version() || std::fabs(st.width - width) > 0.5F) {
    // Keep the top block in place across re-layouts.
    const std::uint32_t top = TopBlock(st);
    Reset(doc, st, width, t);
    if (top < doc.size()) {
      st.scroll = FenwickPrefix(st.fenwick, top);
    }
  }
  const double viewport = std::max(1.0F, rect.Height());

  // Search.
  if (st.search != st.searched || st.searched_version != doc.version()) {
    st.searched = st.search;
    st.searched_version = doc.version();
    st.matches = doc.FindAll(data::ParseTerms(st.search));
    st.current = -1;
    // Jump to the first match at or below the top of the view.
    const std::uint32_t top = TopBlock(st);
    for (std::size_t m = 0; m < st.matches.size(); ++m) {
      if (st.matches[m].block >= top) {
        st.current = static_cast<int>(m);
        break;
      }
    }
    if (st.current < 0 && !st.matches.empty()) {
      st.current = 0;
    }
    st.scroll_to_match = st.current >= 0;
  }

  // Keys.
  if (ctx.HasFocus(id)) {
    if (ctx.ConsumeKey(Key::kDown))
      st.scroll += kLineScroll;
    if (ctx.ConsumeKey(Key::kUp))
      st.scroll -= kLineScroll;
    if (ctx.ConsumeKey(Key::kPageDown) || ctx.ConsumeKey(Key::kSpace)) {
      st.scroll += viewport * 0.9;
    }
    if (ctx.ConsumeKey(Key::kPageUp))
      st.scroll -= viewport * 0.9;
    if (ctx.ConsumeKey(Key::kHome))
      st.scroll = 0.0;
    if (ctx.ConsumeKey(Key::kEnd))
      st.scroll = 1e300;
  }
  if (ctx.ConsumeKey(Key::kF3) && ctx.HasFocus(id)) {
    GoToMatch(st, false);
  }
  if (ctx.ConsumeKey(Key::kF3, kModShift) && ctx.HasFocus(id)) {
    GoToMatch(st, true);
  }
  st.scroll -= ctx.Wheel(id).y * kLineScroll;

  // Jumps to a block or the current match.
  if (st.scroll_to_block && *st.scroll_to_block < doc.size()) {
    EnsureLayout(ctx, doc, st, *st.scroll_to_block);
    st.scroll = FenwickPrefix(st.fenwick, *st.scroll_to_block);
    st.scroll_to_block.reset();
  }
  if (st.scroll_to_match && st.current >= 0 &&
      st.current < static_cast<int>(st.matches.size())) {
    const data::DocMatch &m = st.matches[static_cast<std::size_t>(st.current)];
    // Lay out the blocks above so the offset is exact near the target.
    for (std::uint32_t b = m.block >= 8 ? m.block - 8 : 0; b <= m.block; ++b) {
      EnsureLayout(ctx, doc, st, b);
    }
    const text::RichLayout &l = EnsureLayout(ctx, doc, st, m.block);
    const auto rects = l.RangeRects(m.range.begin, m.range.end);
    const double y = FenwickPrefix(st.fenwick, m.block) +
                     TextTop(doc.blocks()[m.block]) +
                     (rects.empty() ? 0.0 : rects.front().y0);
    if (y < st.scroll || y > st.scroll + viewport - 40) {
      st.scroll = y - viewport / 3;
    }
    st.scroll_to_match = false;
  }

  // Lay out what is visible; exact heights replace estimates. The block
  // at the top stays put while heights above it change.
  const double total = FenwickPrefix(st.fenwick, doc.size());
  st.scroll = std::clamp(st.scroll, 0.0, std::max(0.0, total - viewport));
  if (doc.size() > 0) {
    const std::uint32_t top = TopBlock(st);
    const double into = st.scroll - FenwickPrefix(st.fenwick, top);
    for (std::uint32_t b = top; b < doc.size(); ++b) {
      const double y = FenwickPrefix(st.fenwick, b) - st.scroll;
      if (y > viewport) {
        break;
      }
      EnsureLayout(ctx, doc, st, b);
    }
    st.scroll = FenwickPrefix(st.fenwick, top) + into;
    const double total_after = FenwickPrefix(st.fenwick, doc.size());
    st.scroll =
        std::clamp(st.scroll, 0.0, std::max(0.0, total_after - viewport));
    // Bound the layout cache to blocks near the view.
    if (st.layouts.size() > 400) {
      std::erase_if(st.layouts, [&](const auto &item) {
        return item.first + 50 < top || item.first > top + 250;
      });
      std::erase_if(st.runs, [&](const auto &item) {
        return !st.layouts.contains(item.first);
      });
    }
  }

  // Links.
  if (it.clicked && doc.size() > 0) {
    const double y = st.scroll + it.local.y;
    const auto b = static_cast<std::uint32_t>(FenwickFind(st.fenwick, y));
    auto found = st.layouts.find(b);
    if (found != st.layouts.end()) {
      const DocBlock &block = doc.blocks()[b];
      const float local_y =
          static_cast<float>(y - FenwickPrefix(st.fenwick, b) - TextTop(block));
      const float indent =
          BoxOf(block).indent * (block.kind == BlockKind::kCode ? 2.0F : 1.0F);
      const std::uint32_t byte =
          found->second.ByteAt({it.local.x - kPad - indent, local_y});
      for (const data::DocSpan &s : block.spans) {
        if (!s.link.empty() && byte >= s.begin && byte < s.end) {
          result.link = s.link;
        }
      }
    }
  }

  if (layout.width.kind == SizeKind::kFit) {
    layout.width = SizeSpec::Grow();
  }
  if (layout.height.kind == SizeKind::kFit) {
    layout.height = SizeSpec::Grow();
  }
  NodeDesc outer;
  outer.layout = layout;
  outer.layout.mode = LayoutMode::kRow;
  outer.layout.padding = Padding::All(1);
  outer.style.fill = t.colors.surface;
  outer.style.border = t.colors.border;
  outer.style.border_width = t.metrics.border_width;
  outer.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  ctx.Open(outer);
  NodeDesc body;
  body.id = id;
  body.layout.width = SizeSpec::Grow();
  body.layout.height = SizeSpec::Grow();
  body.flags = kNodeInteractive | kNodeFocusable | kNodeWheel | kNodeClip |
               kNodeFocusRing;
  const int node = ctx.Open(body);
  ctx.Close();
  ctx.AddPainter(node, [&doc, &st](PaintContext &pc, const Rect &r) {
    PaintDocument(pc, r, doc, st);
  });
  detail::ScrollBar(
      ctx, bar_id, st.scroll, FenwickPrefix(st.fenwick, doc.size()), viewport);
  ctx.Close();
  return result;
}

}  // namespace gloomy::ui
