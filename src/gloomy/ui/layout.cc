#include "gloomy/ui/layout.h"

#include <algorithm>
#include <cmath>

namespace gloomy::ui {

namespace {

auto Clamp(float v, const SizeSpec &s) -> float {
  return std::clamp(v, s.min, std::max(s.min, s.max));
}

// Applies a size spec to content sizes along one axis.
void ApplySpec(const SizeSpec &s, float content_min, float content_pref,
               float &min, float &pref) {
  switch (s.kind) {
    case SizeKind::kFixed:
      min = std::min(s.min, s.value);
      pref = s.value;
      break;
    case SizeKind::kPercent:
      min = s.min;
      pref = Clamp(content_pref, s);
      break;
    case SizeKind::kFit:
    case SizeKind::kGrow:
      min = Clamp(content_min, s);
      pref = Clamp(content_pref, s);
      break;
  }
}

// Size of a child along the cross axis.
auto CrossSize(const SizeSpec &s, float child_min, float child_pref,
               float inner, Align align, bool scrolls) -> float {
  switch (s.kind) {
    case SizeKind::kFixed:
      return s.value;
    case SizeKind::kPercent:
      return Clamp(inner * s.value, s);
    case SizeKind::kGrow:
      return Clamp(inner, s);
    case SizeKind::kFit:
      if (align == Align::kStretch) {
        return Clamp(inner, s);
      }
      if (scrolls) {
        return child_pref;
      }
      return std::max(child_min, std::min(child_pref, inner));
  }
  return child_pref;
}

}  // namespace

void LayoutTree::DistributeMain(int index, bool horizontal, float available) {
  // Children's current size along the axis (w or h) is their base size.
  const LayoutNode &n = nodes_[index];
  auto size = [&](LayoutNode &c) -> float & {
    return horizontal ? c.w : c.h;
  };
  auto spec = [&](const LayoutNode &c) -> const SizeSpec & {
    return horizontal ? c.spec.width : c.spec.height;
  };
  auto min = [&](const LayoutNode &c) {
    return horizontal ? c.min_w : c.min_h;
  };
  float total = 0.0F;
  for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
    total += size(nodes_[c]);
  }
  if (total < available) {
    // Grow: share leftover by weight, freezing children that hit max.
    frozen_.assign(static_cast<std::size_t>(n.child_count), 0);
    float extra = available - total;
    for (int round = 0; round < 8 && extra > 0.01F; ++round) {
      float weights = 0.0F;
      int i = 0;
      for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling, ++i) {
        if (frozen_[i] == 0 && spec(nodes_[c]).kind == SizeKind::kGrow) {
          weights += spec(nodes_[c]).value;
        }
      }
      if (weights <= 0.0F) {
        break;
      }
      float used = 0.0F;
      i = 0;
      for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling, ++i) {
        LayoutNode &child = nodes_[c];
        const SizeSpec &cs = spec(child);
        if (frozen_[i] != 0 || cs.kind != SizeKind::kGrow) {
          continue;
        }
        const float share = extra * cs.value / weights;
        const float grown = std::min(size(child) + share, cs.max);
        if (grown < size(child) + share) {
          frozen_[i] = 1;
        }
        used += grown - size(child);
        size(child) = grown;
      }
      extra -= used;
    }
  } else if (total > available) {
    // Shrink: take space from children in proportion to what they can give.
    // Fixed sizes give only what lies above their min (Preferred()).
    float capacity = 0.0F;
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      const LayoutNode &child = nodes_[c];
      capacity += std::max(0.0F, size(nodes_[c]) - min(child));
    }
    if (capacity <= 0.0F) {
      return;
    }
    const float take = std::min(1.0F, (total - available) / capacity);
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      size(child) -= std::max(0.0F, size(child) - min(child)) * take;
    }
  }
}

auto LayoutTree::Add(int parent, const LayoutSpec &spec, int measure) -> int {
  const int index = static_cast<int>(nodes_.size());
  LayoutNode &node = nodes_.emplace_back();
  node.spec = spec;
  node.parent = parent;
  node.measure = measure;
  if (parent >= 0) {
    LayoutNode &p = nodes_[parent];
    if (p.last_child >= 0) {
      nodes_[p.last_child].next_sibling = index;
    } else {
      p.first_child = index;
    }
    p.last_child = index;
    ++p.child_count;
  }
  return index;
}

void LayoutTree::FitWidths(int root, Measurer &measurer) {
  // order_ holds the subtree in pre-order; walk it backwards so children
  // come before parents.
  order_.clear();
  order_.push_back(root);
  for (std::size_t i = 0; i < order_.size(); ++i) {
    for (int c = nodes_[order_[i]].first_child; c >= 0;
         c = nodes_[c].next_sibling) {
      order_.push_back(c);
    }
  }
  for (auto it = order_.rbegin(); it != order_.rend(); ++it) {
    LayoutNode &n = nodes_[*it];
    const LayoutSpec &s = n.spec;
    const float pad = s.padding.left + s.padding.right;
    float cmin = 0.0F;
    float cpref = 0.0F;
    if (n.measure >= 0) {
      const WidthRange range = measurer.MeasureWidth(n.measure);
      cmin = range.min;
      cpref = range.pref;
    } else if (n.child_count > 0) {
      const float gaps = s.gap * static_cast<float>(n.child_count - 1);
      for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
        const LayoutNode &child = nodes_[c];
        if (s.mode == LayoutMode::kRow) {
          cmin += child.min_w;
          cpref += child.pref_w;
        } else {
          cmin = std::max(cmin, child.min_w);
          cpref = std::max(cpref, child.pref_w);
        }
      }
      if (s.mode == LayoutMode::kRow) {
        cmin += gaps;
        cpref += gaps;
      } else if (s.mode == LayoutMode::kGrid) {
        const int cols = std::max(1, s.grid_columns);
        const float col_gaps = s.gap * static_cast<float>(cols - 1);
        cmin = cmin * static_cast<float>(cols) + col_gaps;
        cpref = cpref * static_cast<float>(cols) + col_gaps;
      }
    }
    if (s.scroll_x || s.shrink_x) {
      cmin = 0.0F;
    }
    ApplySpec(s.width, cmin + pad, cpref + pad, n.min_w, n.pref_w);
  }
}

void LayoutTree::AssignWidths(int index) {
  LayoutNode &n = nodes_[index];
  const LayoutSpec &s = n.spec;
  if (n.child_count == 0) {
    return;
  }
  const float inner = std::max(0.0F, n.w - s.padding.left - s.padding.right);
  if (s.mode == LayoutMode::kRow) {
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      const SizeSpec &cs = child.spec.width;
      child.w = child.pref_w;
      if (cs.kind == SizeKind::kFixed) {
        child.w = cs.value;
      } else if (cs.kind == SizeKind::kPercent) {
        child.w = Clamp(inner * cs.value, cs);
      }
    }
    if (!s.scroll_x) {
      const float gaps = s.gap * static_cast<float>(n.child_count - 1);
      DistributeMain(index, true, inner - gaps);
    }
  } else if (s.mode == LayoutMode::kGrid) {
    const int cols = std::max(1, s.grid_columns);
    const float col_w = (inner - s.gap * static_cast<float>(cols - 1)) /
                        static_cast<float>(cols);
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      child.w = child.spec.width.kind == SizeKind::kFixed
                    ? child.spec.width.value
                    : std::max(0.0F, col_w);
    }
  } else {
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      child.w = CrossSize(child.spec.width,
                          child.min_w,
                          child.pref_w,
                          inner,
                          s.align,
                          s.scroll_x);
    }
  }
  for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
    AssignWidths(c);
  }
}

void LayoutTree::FitHeights(int root, Measurer &measurer) {
  (void)root;
  // order_ still holds the subtree from FitWidths().
  for (auto it = order_.rbegin(); it != order_.rend(); ++it) {
    LayoutNode &n = nodes_[*it];
    const LayoutSpec &s = n.spec;
    const float pad = s.padding.top + s.padding.bottom;
    float cmin = 0.0F;
    float cpref = 0.0F;
    if (n.measure >= 0) {
      const float h = measurer.MeasureHeight(
          n.measure, std::max(0.0F, n.w - s.padding.left - s.padding.right));
      cmin = h;
      cpref = h;
    } else if (n.child_count > 0) {
      if (s.mode == LayoutMode::kGrid) {
        const int cols = std::max(1, s.grid_columns);
        int i = 0;
        float row_min = 0.0F;
        float row_pref = 0.0F;
        int rows = 0;
        for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
          row_min = std::max(row_min, nodes_[c].min_h);
          row_pref = std::max(row_pref, nodes_[c].pref_h);
          if (++i % cols == 0 || nodes_[c].next_sibling < 0) {
            cmin += row_min;
            cpref += row_pref;
            row_min = 0.0F;
            row_pref = 0.0F;
            ++rows;
          }
        }
        cmin += s.gap * static_cast<float>(rows - 1);
        cpref += s.gap * static_cast<float>(rows - 1);
      } else {
        for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
          const LayoutNode &child = nodes_[c];
          if (s.mode == LayoutMode::kColumn) {
            cmin += child.min_h;
            cpref += child.pref_h;
          } else {
            cmin = std::max(cmin, child.min_h);
            cpref = std::max(cpref, child.pref_h);
          }
        }
        if (s.mode == LayoutMode::kColumn) {
          const float gaps = s.gap * static_cast<float>(n.child_count - 1);
          cmin += gaps;
          cpref += gaps;
        }
      }
    }
    if (s.scroll_y) {
      cmin = 0.0F;
    }
    ApplySpec(s.height, cmin + pad, cpref + pad, n.min_h, n.pref_h);
  }
}

void LayoutTree::AssignHeights(int index) {
  LayoutNode &n = nodes_[index];
  const LayoutSpec &s = n.spec;
  if (n.child_count == 0) {
    return;
  }
  const float inner = std::max(0.0F, n.h - s.padding.top - s.padding.bottom);
  if (s.mode == LayoutMode::kColumn) {
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      const SizeSpec &cs = child.spec.height;
      child.h = child.pref_h;
      if (cs.kind == SizeKind::kFixed) {
        child.h = cs.value;
      } else if (cs.kind == SizeKind::kPercent) {
        child.h = Clamp(inner * cs.value, cs);
      }
    }
    if (!s.scroll_y) {
      const float gaps = s.gap * static_cast<float>(n.child_count - 1);
      DistributeMain(index, false, inner - gaps);
    }
  } else if (s.mode == LayoutMode::kGrid) {
    // Each child takes its row's height.
    const int cols = std::max(1, s.grid_columns);
    int row_start = n.first_child;
    while (row_start >= 0) {
      float row_h = 0.0F;
      int c = row_start;
      for (int i = 0; i < cols && c >= 0; ++i, c = nodes_[c].next_sibling) {
        row_h = std::max(row_h, nodes_[c].pref_h);
      }
      c = row_start;
      for (int i = 0; i < cols && c >= 0; ++i, c = nodes_[c].next_sibling) {
        nodes_[c].h = nodes_[c].spec.height.kind == SizeKind::kFixed
                          ? nodes_[c].spec.height.value
                          : row_h;
      }
      row_start = c;
    }
  } else {
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      child.h = CrossSize(child.spec.height,
                          child.min_h,
                          child.pref_h,
                          inner,
                          s.align,
                          s.scroll_y);
    }
  }
  for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
    AssignHeights(c);
  }
}

void LayoutTree::Position(int index) {
  LayoutNode &n = nodes_[index];
  const LayoutSpec &s = n.spec;
  const Rect inner{n.rect.x0 + s.padding.left,
                   n.rect.y0 + s.padding.top,
                   n.rect.x1 - s.padding.right,
                   n.rect.y1 - s.padding.bottom};
  const float ox = inner.x0 - s.scroll.x;
  const float oy = inner.y0 - s.scroll.y;
  const float inner_w = inner.Width();
  const float inner_h = inner.Height();

  auto cross = [&](float avail, float size) -> float {
    switch (s.align) {
      case Align::kCenter:
        return std::round((avail - size) * 0.5F);
      case Align::kEnd:
        return avail - size;
      default:
        return 0.0F;
    }
  };

  float extent_x = 0.0F;
  float extent_y = 0.0F;
  if (s.mode == LayoutMode::kRow || s.mode == LayoutMode::kColumn) {
    const bool row = s.mode == LayoutMode::kRow;
    float total = 0.0F;
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      total += row ? nodes_[c].w : nodes_[c].h;
    }
    total += s.gap * static_cast<float>(std::max(0, n.child_count - 1));
    const float free = std::max(0.0F, (row ? inner_w : inner_h) - total);
    float pos = 0.0F;
    float gap = s.gap;
    switch (s.justify) {
      case Justify::kCenter:
        pos = std::round(free * 0.5F);
        break;
      case Justify::kEnd:
        pos = free;
        break;
      case Justify::kSpaceBetween:
        if (n.child_count > 1) {
          gap += free / static_cast<float>(n.child_count - 1);
        }
        break;
      default:
        break;
    }
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      float x = 0.0F;
      float y = 0.0F;
      if (row) {
        x = pos;
        y = cross(inner_h, child.h);
        pos += child.w + gap;
      } else {
        x = cross(inner_w, child.w);
        y = pos;
        pos += child.h + gap;
      }
      child.rect = Rect::FromXywh(ox + x, oy + y, child.w, child.h);
      extent_x = std::max(extent_x, x + child.w);
      extent_y = std::max(extent_y, y + child.h);
    }
  } else if (s.mode == LayoutMode::kStack) {
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      const float x = cross(inner_w, child.w);
      const float y = cross(inner_h, child.h);
      child.rect = Rect::FromXywh(ox + x, oy + y, child.w, child.h);
      extent_x = std::max(extent_x, x + child.w);
      extent_y = std::max(extent_y, y + child.h);
    }
  } else {
    const int cols = std::max(1, s.grid_columns);
    const float col_w = (inner_w - s.gap * static_cast<float>(cols - 1)) /
                        static_cast<float>(cols);
    int i = 0;
    float y = 0.0F;
    float row_h = 0.0F;
    for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
      LayoutNode &child = nodes_[c];
      const int col = i % cols;
      if (col == 0 && i > 0) {
        y += row_h + s.gap;
        row_h = 0.0F;
      }
      const float x = static_cast<float>(col) * (col_w + s.gap);
      child.rect = Rect::FromXywh(ox + x, oy + y, child.w, child.h);
      row_h = std::max(row_h, child.h);
      extent_x = std::max(extent_x, x + child.w);
      extent_y = std::max(extent_y, y + child.h);
      ++i;
    }
  }
  n.content = {extent_x, extent_y};
  for (int c = n.first_child; c >= 0; c = nodes_[c].next_sibling) {
    Position(c);
  }
}

void LayoutTree::Compute(int root, const Rect &available, Measurer &measurer) {
  LayoutNode &r = nodes_[root];
  FitWidths(root, measurer);
  const SizeSpec &ws = r.spec.width;
  if (ws.kind == SizeKind::kFixed) {
    r.w = ws.value;
  } else if (ws.kind == SizeKind::kFit) {
    r.w = std::min(r.pref_w, available.Width());
  } else if (ws.kind == SizeKind::kPercent) {
    r.w = Clamp(available.Width() * ws.value, ws);
  } else {
    r.w = Clamp(available.Width(), ws);
  }
  AssignWidths(root);
  FitHeights(root, measurer);
  const SizeSpec &hs = r.spec.height;
  if (hs.kind == SizeKind::kFixed) {
    r.h = hs.value;
  } else if (hs.kind == SizeKind::kFit) {
    r.h = std::min(r.pref_h, available.Height());
  } else if (hs.kind == SizeKind::kPercent) {
    r.h = Clamp(available.Height() * hs.value, hs);
  } else {
    r.h = Clamp(available.Height(), hs);
  }
  AssignHeights(root);
  r.rect = Rect::FromXywh(available.x0, available.y0, r.w, r.h);
  Position(root);
}

}  // namespace gloomy::ui
