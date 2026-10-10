#include "gloomy/ui/menu_list.h"

#include <algorithm>
#include <cstdint>

#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

auto MenuListStep(std::span<const MenuListItem> items, int cursor,
                  int direction) -> int {
  const int n = static_cast<int>(items.size());
  if (n == 0) {
    return -1;
  }
  for (int k = 1; k <= n; ++k) {
    const int next = (((cursor + direction * k) % n) + n) % n;
    if (items[static_cast<std::size_t>(next)].enabled) {
      return next;
    }
  }
  return cursor;
}

auto MenuList(Context &ctx, std::string_view label,
              std::span<const MenuListItem> items, int &cursor,
              const MenuListOptions &options) -> int {
  const int n = static_cast<int>(items.size());
  if (n == 0) {
    return -1;
  }
  cursor = std::clamp(cursor, 0, n - 1);
  if (!items[static_cast<std::size_t>(cursor)].enabled) {
    cursor = MenuListStep(items, cursor, 1);
  }
  int activated = -1;
  if (options.keys) {
    if (ctx.ConsumeKey(Key::kDown)) {
      cursor = MenuListStep(items, cursor, 1);
    }
    if (ctx.ConsumeKey(Key::kUp)) {
      cursor = MenuListStep(items, cursor, -1);
    }
    if (ctx.ConsumeKey(Key::kEnter) &&
        items[static_cast<std::size_t>(cursor)].enabled) {
      activated = cursor;
    }
  }
  const Theme &t = ctx.theme();
  const Color accent = t.colors.selection_accent.a > 0.0F
                           ? t.colors.selection_accent
                           : t.colors.primary;
  const float label_size = options.label_size > 0.0F
                               ? options.label_size
                               : t.metrics.font_size + 5.0F;
  ctx.PushId(label);
  LayoutSpec col;
  col.gap = 2;
  col.width = options.width;
  col.align = Align::kStretch;
  BeginColumn(ctx, col);
  for (int i = 0; i < n; ++i) {
    const MenuListItem &item = items[static_cast<std::size_t>(i)];
    const Id id = ctx.GetId(static_cast<std::uint64_t>(i));
    const Interaction in = ctx.Interact(id);
    if (in.hovered && item.enabled) {
      cursor = i;
    }
    if (in.clicked && item.enabled) {
      cursor = i;
      activated = i;
    }
    const bool current = i == cursor;
    NodeDesc row;
    row.id = id;
    row.flags = kNodeInteractive;
    row.layout.mode = LayoutMode::kRow;
    row.layout.width = SizeSpec::Grow();
    row.layout.gap = 10;
    row.layout.align = Align::kStretch;
    row.style.fill = current ? t.colors.selection : Color{0, 0, 0, 0};
    ctx.Open(row);
    NodeDesc bar;
    bar.layout.width = SizeSpec::Fixed(3.0F);
    bar.layout.height = SizeSpec::Grow();
    bar.style.fill = current ? accent : Color{0, 0, 0, 0};
    ctx.Open(bar);
    ctx.Close();
    LayoutSpec text;
    text.padding = Padding::Xy(4, 6);
    text.gap = 2;
    text.width = SizeSpec::Grow();
    BeginColumn(ctx, text);
    LabelOptions big;
    big.size = label_size;
    big.bold = current;
    big.color = item.enabled ? t.colors.text : t.colors.text_disabled;
    Label(ctx, item.label, big);
    const std::string &detail =
        item.enabled ? item.detail : item.disabled_reason;
    if (!detail.empty()) {
      LabelOptions small;
      small.size = t.metrics.small_font_size + 1.0F;
      small.color = current ? t.colors.text : t.colors.text_muted;
      Label(ctx, detail, small);
    }
    End(ctx);
    ctx.Close();
  }
  End(ctx);
  ctx.PopId();
  return activated;
}

}  // namespace gloomy::ui
