#include "ui/widget_util.h"

#include <algorithm>

namespace gloomy::ui::detail {

namespace {

// State of a scroll bar drag.
struct ScrollBarState {
  bool dragging = false;
  double start_offset = 0.0;
};

}  // namespace

auto Tint(const Color &base, const Color &overlay) -> Color {
  const float a = overlay.a;
  return {base.r + (overlay.r - base.r) * a,
          base.g + (overlay.g - base.g) * a,
          base.b + (overlay.b - base.b) * a,
          base.a};
}

auto InteractiveFill(const Theme &t, const Color &base, const Interaction &it)
    -> Color {
  if (it.pressed) {
    return Tint(base, t.colors.active);
  }
  if (it.hovered) {
    return Tint(base, t.colors.hover);
  }
  return base;
}

auto ThumbRect(const Rect &track, double offset, double content,
               double viewport) -> Rect {
  if (content <= viewport || content <= 0.0) {
    return {};
  }
  const double h = track.Height();
  const double thumb = std::max(20.0, h * viewport / content);
  const double y = (h - thumb) * offset / (content - viewport);
  return Rect{track.x0 + 2,
              static_cast<float>(track.y0 + y),
              track.x1 - 2,
              static_cast<float>(track.y0 + y + thumb)};
}

auto ScrollBar(Context &ctx, Id id, double &offset, double content,
               double viewport) -> bool {
  const Theme &t = ctx.theme();
  ScrollBarState &state = ctx.State<ScrollBarState>(id);
  const Interaction it = ctx.Interact(id);
  const double max_offset = std::max(0.0, content - viewport);
  const double before = offset;
  if (it.pressed && max_offset > 0.0) {
    const Rect thumb = ThumbRect(it.rect, offset, content, viewport);
    if (!state.dragging) {
      state.dragging = true;
      state.start_offset = offset;
      if (!thumb.Contains(ctx.mouse())) {
        // Click on the track: page towards the pointer.
        offset += (ctx.mouse().y < thumb.y0 ? -1.0 : 1.0) * viewport;
        state.start_offset = offset;
      }
    } else {
      const double track = it.rect.Height() - thumb.Height();
      if (track > 0.0) {
        offset = state.start_offset + it.drag.y * max_offset / track;
      }
    }
  } else {
    if (it.clicked && !state.dragging && max_offset > 0.0) {
      // Pressed and released within one frame (a quick click at a low
      // frame rate): page as a press on the track would.
      const Rect thumb = ThumbRect(it.rect, offset, content, viewport);
      if (!thumb.Contains(ctx.mouse())) {
        offset += (ctx.mouse().y < thumb.y0 ? -1.0 : 1.0) * viewport;
      }
    }
    state.dragging = false;
  }
  offset = std::clamp(offset, 0.0, max_offset);

  NodeDesc bar;
  bar.id = id;
  bar.layout.width = SizeSpec::Fixed(t.metrics.scrollbar_width);
  bar.layout.height = SizeSpec::Grow();
  bar.flags = kNodeInteractive;
  const int node = ctx.Open(bar);
  ctx.Close();
  const Color thumb_color =
      it.pressed ? t.colors.text_muted
                 : (it.hovered ? t.colors.text_disabled : t.colors.border);
  const double off = offset;
  ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
    const Rect thumb = ThumbRect(r, off, content, viewport);
    if (!thumb.Empty()) {
      pc.list.FillRect(
          thumb, thumb_color, gfx::CornerRadii::All(thumb.Width() * 0.5F));
    }
  });
  return offset != before;
}

}  // namespace gloomy::ui::detail
