#ifndef GLOOMY_UI_WIDGET_UTIL_H_
#define GLOOMY_UI_WIDGET_UTIL_H_

// Helpers shared by the widget implementations; not public API.

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/core/id.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/theme.h"

namespace gloomy::ui::detail {

/// Composites `overlay` (with its alpha) over the opaque `base`.
auto Tint(const Color &base, const Color &overlay) -> Color;

/// `base` tinted for hover or press.
auto InteractiveFill(const Theme &t, const Color &base, const Interaction &it)
    -> Color;

/// Geometry of a vertical scroll bar thumb inside `track`; empty when
/// everything fits.
auto ThumbRect(const Rect &track, double offset, double content,
               double viewport) -> Rect;

/// Declares a vertical scroll bar node `id`. Dragging the thumb and
/// clicking the track update `offset` (clamped to the content).
/// @return True if `offset` changed.
auto ScrollBar(Context &ctx, Id id, double &offset, double content,
               double viewport) -> bool;

}  // namespace gloomy::ui::detail

#endif  // GLOOMY_UI_WIDGET_UTIL_H_
