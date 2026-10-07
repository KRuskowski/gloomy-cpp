#include "gloomy/core/atlas.h"

#include <algorithm>
#include <climits>
#include <cstring>

namespace gloomy {

namespace {

// Padding around each region so bilinear sampling never bleeds.
constexpr int kPadding = 1;

}  // namespace

Atlas::Atlas(int page_size, int max_pages)
    : page_size_(page_size), max_pages_(max_pages) {
  AddPage();
}

void Atlas::AddPage() {
  Page page;
  page.pixels.assign(static_cast<std::size_t>(page_size_) * page_size_, 0);
  page.skyline.push_back({0, 0, page_size_});
  pages_.push_back(std::move(page));
}

auto Atlas::Fit(const Page &page, std::size_t index, int w, int h) const
    -> int {
  // Returns the y at which a w x h box placed at skyline[index].x rests, or
  // -1 if it does not fit.
  const int x = page.skyline[index].x;
  if (x + w > page_size_) {
    return -1;
  }
  int y = 0;
  int remaining = w;
  std::size_t i = index;
  while (remaining > 0) {
    if (i >= page.skyline.size()) {
      return -1;
    }
    y = std::max(y, page.skyline[i].y);
    if (y + h > page_size_) {
      return -1;
    }
    remaining -= page.skyline[i].w;
    ++i;
  }
  return y;
}

auto Atlas::AllocateOn(int page_index, int w, int h)
    -> std::optional<AtlasRegion> {
  Page &page = pages_[page_index];
  int best_y = INT_MAX;
  int best_w = INT_MAX;
  std::size_t best = page.skyline.size();
  for (std::size_t i = 0; i < page.skyline.size(); ++i) {
    const int y = Fit(page, i, w, h);
    if (y < 0) {
      continue;
    }
    // Lowest resting place first, then the narrowest segment.
    if (y + h < best_y || (y + h == best_y && page.skyline[i].w < best_w)) {
      best_y = y + h;
      best_w = page.skyline[i].w;
      best = i;
    }
  }
  if (best == page.skyline.size()) {
    return std::nullopt;
  }
  const int x = page.skyline[best].x;
  const int y = best_y - h;
  // Insert the new segment and trim the ones it covers.
  page.skyline.insert(page.skyline.begin() + static_cast<long>(best),
                      Segment{x, y + h, w});
  for (std::size_t i = best + 1; i < page.skyline.size();) {
    Segment &s = page.skyline[i];
    const Segment &prev = page.skyline[i - 1];
    if (s.x >= prev.x + prev.w) {
      break;
    }
    const int shrink = prev.x + prev.w - s.x;
    s.x += shrink;
    s.w -= shrink;
    if (s.w <= 0) {
      page.skyline.erase(page.skyline.begin() + static_cast<long>(i));
    } else {
      break;
    }
  }
  // Merge neighbours at the same height.
  for (std::size_t i = 0; i + 1 < page.skyline.size();) {
    if (page.skyline[i].y == page.skyline[i + 1].y) {
      page.skyline[i].w += page.skyline[i + 1].w;
      page.skyline.erase(page.skyline.begin() + static_cast<long>(i) + 1);
    } else {
      ++i;
    }
  }
  return AtlasRegion{page_index, x, y, w, h};
}

auto Atlas::Allocate(int w, int h) -> std::optional<AtlasRegion> {
  const int pw = w + 2 * kPadding;
  const int ph = h + 2 * kPadding;
  if (pw > page_size_ || ph > page_size_) {
    return std::nullopt;
  }
  for (int p = 0;; ++p) {
    if (p == page_count()) {
      if (page_count() >= max_pages_) {
        return std::nullopt;
      }
      AddPage();
    }
    if (auto region = AllocateOn(p, pw, ph)) {
      region->x += kPadding;
      region->y += kPadding;
      region->w = w;
      region->h = h;
      return region;
    }
  }
}

void Atlas::Write(const AtlasRegion &region, const std::uint8_t *src,
                  int pitch) {
  Page &page = pages_[region.page];
  for (int row = 0; row < region.h; ++row) {
    std::memcpy(page.pixels.data() +
                    static_cast<std::size_t>(region.y + row) * page_size_ +
                    region.x,
                src + static_cast<std::ptrdiff_t>(row) * pitch,
                static_cast<std::size_t>(region.w));
  }
  const AtlasDirtyRect r{region.page,
                         region.x,
                         region.y,
                         region.x + region.w,
                         region.y + region.h};
  if (!page.dirty) {
    page.dirty = r;
  } else {
    page.dirty->x0 = std::min(page.dirty->x0, r.x0);
    page.dirty->y0 = std::min(page.dirty->y0, r.y0);
    page.dirty->x1 = std::max(page.dirty->x1, r.x1);
    page.dirty->y1 = std::max(page.dirty->y1, r.y1);
  }
}

auto Atlas::TakeDirty() -> std::vector<AtlasDirtyRect> {
  std::vector<AtlasDirtyRect> dirty;
  for (Page &page : pages_) {
    if (page.dirty) {
      dirty.push_back(*page.dirty);
      page.dirty.reset();
    }
  }
  return dirty;
}

void Atlas::Clear() {
  for (std::size_t p = 0; p < pages_.size(); ++p) {
    Page &page = pages_[p];
    std::fill(page.pixels.begin(), page.pixels.end(), 0);
    page.skyline.assign(1, Segment{0, 0, page_size_});
    page.dirty =
        AtlasDirtyRect{static_cast<int>(p), 0, 0, page_size_, page_size_};
  }
  ++generation_;
}

}  // namespace gloomy
