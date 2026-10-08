#include "gloomy/icon/icon_set.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

// nanosvg is C; its implementation is compiled here only.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace gloomy::icon {

namespace {

struct ImageDeleter {
  void operator()(NSVGimage *image) const { nsvgDelete(image); }
};

struct RasterizerDeleter {
  void operator()(NSVGrasterizer *r) const { nsvgDeleteRasterizer(r); }
};

using ImagePtr = std::unique_ptr<NSVGimage, ImageDeleter>;

}  // namespace

struct IconSet::Impl {
  struct Entry {
    std::string name;
    ImagePtr image;
  };

  std::vector<Entry> icons;
  std::unique_ptr<NSVGrasterizer, RasterizerDeleter> rasterizer{
      nsvgCreateRasterizer()};
};

IconSet::IconSet() : impl_(std::make_unique<Impl>()) {}

IconSet::IconSet(IconSet &&) noexcept = default;

auto IconSet::operator=(IconSet &&) noexcept -> IconSet & = default;

IconSet::~IconSet() = default;

auto IconSet::Add(std::string_view name, std::string_view svg) -> IconId {
  // nsvgParse() writes into its input.
  std::string text(svg);
  ImagePtr image(nsvgParse(text.data(), "px", 96.0F));
  if (!image || image->shapes == nullptr || image->width <= 0.0F ||
      image->height <= 0.0F) {
    return kNoIcon;
  }
  const IconId existing = Find(name);
  if (existing != kNoIcon) {
    impl_->icons[existing].image = std::move(image);
    ++version_;
    return existing;
  }
  impl_->icons.push_back({std::string(name), std::move(image)});
  return static_cast<IconId>(impl_->icons.size() - 1);
}

auto IconSet::Find(std::string_view name) const -> IconId {
  for (std::size_t i = 0; i < impl_->icons.size(); ++i) {
    if (impl_->icons[i].name == name) {
      return static_cast<IconId>(i);
    }
  }
  return kNoIcon;
}

auto IconSet::Name(IconId id) const -> const std::string & {
  return impl_->icons.at(id).name;
}

auto IconSet::size() const -> std::size_t {
  return impl_->icons.size();
}

auto IconSet::Rasterise(IconId id, int px, float dx, float dy) const
    -> IconImage {
  IconImage out;
  if (id >= impl_->icons.size() || px < 1) {
    return out;
  }
  NSVGimage *image = impl_->icons[id].image.get();
  const float scale =
      static_cast<float>(px) / std::max(image->width, image->height);
  // Centre a non-square view box.
  const float tx = dx + 0.5F * (static_cast<float>(px) - image->width * scale);
  const float ty = dy + 0.5F * (static_cast<float>(px) - image->height * scale);
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(px) * px * 4);
  nsvgRasterize(impl_->rasterizer.get(),
                image,
                tx,
                ty,
                scale,
                rgba.data(),
                px,
                px,
                px * 4);
  out.width = px;
  out.height = px;
  out.pixels.resize(static_cast<std::size_t>(px) * px);
  for (std::size_t i = 0; i < out.pixels.size(); ++i) {
    out.pixels[i] = rgba[i * 4 + 3];
  }
  return out;
}

auto Dilate(const IconImage &image, int radius) -> IconImage {
  if (radius <= 0 || image.Empty()) {
    return image;
  }
  IconImage out;
  out.width = image.width + 2 * radius;
  out.height = image.height + 2 * radius;
  out.pixels.assign(static_cast<std::size_t>(out.width) * out.height, 0);
  // Offsets inside the disc; the corners of the square are left out from a
  // radius of 2, so halos stay round.
  const float reach = static_cast<float>(radius) + 0.5F;
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      std::uint8_t best = 0;
      for (int oy = -radius; oy <= radius; ++oy) {
        const int sy = y - radius + oy;
        if (sy < 0 || sy >= image.height) {
          continue;
        }
        for (int ox = -radius; ox <= radius; ++ox) {
          const int sx = x - radius + ox;
          if (sx < 0 || sx >= image.width ||
              (radius > 1 && std::hypot(static_cast<float>(ox),
                                        static_cast<float>(oy)) > reach)) {
            continue;
          }
          best = std::max(best, image.At(sx, sy));
        }
      }
      out.pixels[static_cast<std::size_t>(y) * out.width + x] = best;
    }
  }
  return out;
}

}  // namespace gloomy::icon
