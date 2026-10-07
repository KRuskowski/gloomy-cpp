#ifndef GLOOMY_CORE_ATLAS_H_
#define GLOOMY_CORE_ATLAS_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace gloomy {

/// A rectangle inside one atlas page, in texels.
struct AtlasRegion {
  int page = 0;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

/// A part of a page whose pixels changed since the last TakeDirty().
struct AtlasDirtyRect {
  int page = 0;
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
};

/// A CPU-side 8-bit texture atlas made of square pages, packed with a skyline
/// allocator. The renderer mirrors the pages on the GPU and uploads what
/// TakeDirty() reports. Graphics independent, so it is testable without a
/// device.
class Atlas {
 public:
  /// @param[in] page_size Width and height of every page in texels.
  /// @param[in] max_pages Pages are added on demand up to this count.
  Atlas(int page_size, int max_pages);

  /// Reserves a `w` x `h` region with one texel of padding around it.
  /// @return std::nullopt when every page is full; Clear() and retry.
  auto Allocate(int w, int h) -> std::optional<AtlasRegion>;

  /// Copies `region.h` rows of `region.w` bytes into the region.
  /// @param[in] pitch Bytes between rows of `src`.
  void Write(const AtlasRegion &region, const std::uint8_t *src, int pitch);

  /// Returns and resets the changed areas, at most one per page.
  auto TakeDirty() -> std::vector<AtlasDirtyRect>;

  /// Frees everything; regions handed out before become invalid. Bumps
  /// generation() so caches can tell.
  void Clear();

  auto PagePixels(int page) const -> const std::uint8_t * {
    return pages_[page].pixels.data();
  }

  auto page_count() const -> int { return static_cast<int>(pages_.size()); }

  auto page_size() const -> int { return page_size_; }

  auto max_pages() const -> int { return max_pages_; }

  auto generation() const -> std::uint64_t { return generation_; }

 private:
  struct Segment {
    int x = 0;
    int y = 0;
    int w = 0;
  };

  struct Page {
    std::vector<std::uint8_t> pixels;
    std::vector<Segment> skyline;
    std::optional<AtlasDirtyRect> dirty;
  };

  void AddPage();

  auto Fit(const Page &page, std::size_t index, int w, int h) const -> int;

  auto AllocateOn(int page, int w, int h) -> std::optional<AtlasRegion>;

  int page_size_;
  int max_pages_;
  std::vector<Page> pages_;
  std::uint64_t generation_ = 0;
};

}  // namespace gloomy

#endif  // GLOOMY_CORE_ATLAS_H_
