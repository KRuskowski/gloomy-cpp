#ifndef GLOOMY_UI_CANVAS_H_
#define GLOOMY_UI_CANVAS_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// A point in the canvas's own coordinates (map units, km, ...). Doubles,
/// so that large worlds keep their precision when zoomed in.
struct WorldPoint {
  double x = 0.0;
  double y = 0.0;
};

/// The mapping between world and screen for one frame: the world point at
/// the centre of `rect` and the zoom in pixels per world unit. Screen y
/// grows downwards, like world y.
struct CanvasView {
  Rect rect;
  double center_x = 0.0;
  double center_y = 0.0;
  double zoom = 1.0;

  auto ToScreen(double x, double y) const -> Vec2 {
    return {rect.Center().x + static_cast<float>((x - center_x) * zoom),
            rect.Center().y + static_cast<float>((y - center_y) * zoom)};
  }

  auto ToScreen(WorldPoint p) const -> Vec2 { return ToScreen(p.x, p.y); }

  auto ToWorld(Vec2 screen) const -> WorldPoint {
    return {center_x + (screen.x - rect.Center().x) / zoom,
            center_y + (screen.y - rect.Center().y) / zoom};
  }

  /// A length in world units as pixels.
  auto Pixels(double world) const -> float {
    return static_cast<float>(world * zoom);
  }
};

/// Camera of a canvas; owned by the app, so it can be saved and moved
/// (centre on an object, fit a region).
struct CanvasState {
  double x = 0.0;
  double y = 0.0;
  /// Pixels per world unit; 0 fits CanvasOptions' bounds on the next
  /// frame.
  double zoom = 0.0;
  // Drag panning.
  bool panning = false;
  bool dragged = false;
  double pan_x = 0.0;
  double pan_y = 0.0;
};

/// A world rectangle.
struct WorldRect {
  double x0 = 0.0;
  double y0 = 0.0;
  double x1 = 1.0;
  double y1 = 1.0;
};

struct CanvasOptions {
  /// Grows in both directions by default.
  LayoutSpec layout = [] {
    LayoutSpec l;
    l.width = SizeSpec::Grow();
    l.height = SizeSpec::Grow();
    return l;
  }();
  /// What to show when the zoom is 0 (first frame, Home, double click on
  /// empty space when `fit_on_double_click`).
  WorldRect fit;
  /// Pixels around `fit`.
  float fit_margin = 16.0F;
  double min_zoom = 1e-6;
  double max_zoom = 1e6;
  /// Zoom factor per wheel line and per +/- key.
  double zoom_step = 1.15;
  /// Alpha 0: the theme's background colour.
  Color background{0, 0, 0, 0};
};

/// What happened on a canvas this frame.
struct CanvasResult {
  /// Node id of the canvas, for focus and Navigate().
  Id id = kNoId;
  /// The view input was resolved against (the previous frame's rectangle
  /// with this frame's camera).
  CanvasView view;
  bool hovered = false;
  /// A left click that was not the end of a drag.
  bool clicked = false;
  bool double_clicked = false;
  bool right_clicked = false;
  /// The pointer in world coordinates (valid when hovered or clicked).
  WorldPoint pointer;
  Vec2 pointer_screen;
};

/// Paints the canvas content; the list is clipped to the canvas and its
/// background is drawn.
using CanvasPainter = std::function<void(PaintContext &, const CanvasView &)>;

/// A custom-draw canvas with a pan/zoom camera: drag to pan, wheel to zoom
/// around the pointer; while focused, + and - zoom around the centre and
/// Home fits `options.fit`. The app draws through `paint` (called once the
/// canvas's rectangle is known) and picks objects with PickNearest().
/// Arrow keys and Enter are left to the app: give the canvas a
/// NavigationTarget and call Navigate() with the result's id, so a map
/// follows the same keyboard contract as lists (docs/design.md 2.5).
auto Canvas(Context &ctx, std::string_view label, CanvasState &state,
            const CanvasOptions &options, CanvasPainter paint) -> CanvasResult;

/// Sets the camera so that `fit` fills `rect` less `margin` pixels.
void FitCanvas(CanvasState &state, const Rect &rect, const WorldRect &fit,
               float margin);

/// The view of `state` over `rect`.
auto MakeCanvasView(const CanvasState &state, const Rect &rect) -> CanvasView;

/// Something on a canvas that can be picked.
struct PickItem {
  std::uint64_t key = 0;
  WorldPoint pos;
  /// Pick radius in pixels.
  float radius = 8.0F;
  /// Higher layers win over lower ones when both are in reach (ships over
  /// stations over sectors).
  int layer = 0;
};

/// The item under `screen`: of the items within their radius, the highest
/// layer, then the nearest.
auto PickNearest(const CanvasView &view, std::span<const PickItem> items,
                 Vec2 screen) -> std::optional<std::uint64_t>;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_CANVAS_H_
