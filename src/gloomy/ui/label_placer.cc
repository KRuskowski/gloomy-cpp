#include "gloomy/ui/label_placer.h"

#include <algorithm>
#include <array>
#include <numeric>

#include "gloomy/text/text_renderer.h"

namespace gloomy::ui {

namespace {

constexpr float kGap = 3.0F;

auto Overlaps(const Rect &a, const Rect &b) -> bool {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

auto Inside(const Rect &outer, const Rect &inner) -> bool {
  return inner.x0 >= outer.x0 && inner.y0 >= outer.y0 && inner.x1 <= outer.x1 &&
         inner.y1 <= outer.y1;
}

/// Candidate boxes around a marker: the preferred side first, then right,
/// left, above, below.
auto Candidates(const LabelRequest &r, float width, float height)
    -> std::array<Rect, 4> {
  const Vec2 a = r.anchor;
  const float d = r.anchor_radius + kGap;
  // Above and below clear the side positions, so all four can be used.
  const float v = std::max(d, height / 2);
  std::array<Rect, 4> boxes = {
      Rect{a.x + d, a.y - height / 2, a.x + d + width, a.y + height / 2},
      Rect{a.x - d - width, a.y - height / 2, a.x - d, a.y + height / 2},
      Rect{a.x - width / 2, a.y - v - height, a.x + width / 2, a.y - v},
      Rect{a.x - width / 2, a.y + v, a.x + width / 2, a.y + v + height},
  };
  const auto first = static_cast<std::size_t>(r.side);
  std::rotate(boxes.begin(),
              boxes.begin() + static_cast<std::ptrdiff_t>(first),
              boxes.begin() + static_cast<std::ptrdiff_t>(first) + 1);
  return boxes;
}

}  // namespace

auto LabelPlacer::Place(const TextMeasure &measure)
    -> std::vector<PlacedLabel> {
  std::vector<std::size_t> order(requests_.size());
  std::iota(order.begin(), order.end(), 0);
  std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) {
    if (requests_[a].force != requests_[b].force) {
      return requests_[a].force;
    }
    return requests_[a].priority > requests_[b].priority;
  });
  std::vector<PlacedLabel> placed;
  auto blocked = [&](const Rect &box) {
    if (!Inside(bounds_, box)) {
      return true;
    }
    for (const PlacedLabel &p : placed) {
      if (Overlaps(p.rect, box)) {
        return true;
      }
    }
    for (const Rect &o : obstacles_) {
      if (Overlaps(o, box)) {
        return true;
      }
    }
    return false;
  };
  for (const std::size_t i : order) {
    const LabelRequest &r = requests_[i];
    const float height = LineHeight(r.size);
    bool done = false;
    for (int variant = 0; variant < 2 && !done; ++variant) {
      const std::string &text = variant == 0 ? r.text : r.short_text;
      if (text.empty()) {
        continue;
      }
      const float width = measure(text, r.size);
      for (const Rect &box : Candidates(r, width, height)) {
        if (!blocked(box)) {
          placed.push_back({i, box, variant == 1});
          done = true;
          break;
        }
      }
    }
    if (!done && r.force && !r.text.empty()) {
      // Forced: the right side, kept inside the bounds where possible.
      const float width = measure(r.text, r.size);
      Rect box = Candidates(r, width, height)[0];
      if (box.x1 > bounds_.x1) {
        box = Candidates(r, width, height)[1];
      }
      placed.push_back({i, box, false});
    }
  }
  return placed;
}

auto LabelPlacer::Draw(PaintContext &pc) -> std::vector<PlacedLabel> {
  const text::FontId font = pc.theme.fonts.sans;
  std::vector<PlacedLabel> placed =
      Place([&](std::string_view text, float size) {
        return pc.text.Measure(text, font, size);
      });
  for (const PlacedLabel &p : placed) {
    const LabelRequest &r = requests_[p.request];
    pc.text.DrawText(pc.list,
                     p.abbreviated ? r.short_text : r.text,
                     font,
                     r.size,
                     {p.rect.x0, p.rect.y0},
                     r.color);
  }
  return placed;
}

}  // namespace gloomy::ui
