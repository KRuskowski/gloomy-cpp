#include "gloomy/text/font_set.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ot.h>
#include <hb.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "assets/LiberationMono-Regular.ttf.h"
#include "assets/LiberationSans-Bold.ttf.h"
#include "assets/LiberationSans-Regular.ttf.h"

namespace gloomy::text {

FontSet::FontSet() {
  FT_Library library = nullptr;
  FT_Init_FreeType(&library);
  library_ = library;
}

FontSet::~FontSet() {
  for (const std::unique_ptr<Face> &face : faces_) {
    hb_font_destroy(face->hb_font);
    FT_Done_Face(face->ft_face);
  }
  FT_Done_FreeType(static_cast<FT_Library>(library_));
}

auto FontSet::AddFace(std::span<const unsigned char> data, bool is_static)
    -> std::expected<FaceId, std::string> {
  auto face = std::make_unique<Face>();
  if (!is_static) {
    face->owned.assign(data.begin(), data.end());
    data = face->owned;
  }
  if (FT_New_Memory_Face(static_cast<FT_Library>(library_),
                         data.data(),
                         static_cast<FT_Long>(data.size()),
                         0,
                         &face->ft_face) != 0) {
    return std::unexpected("FreeType cannot load the font");
  }
  hb_blob_t *blob = hb_blob_create(reinterpret_cast<const char *>(data.data()),
                                   static_cast<unsigned>(data.size()),
                                   HB_MEMORY_MODE_READONLY,
                                   nullptr,
                                   nullptr);
  hb_face_t *hb_face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);
  face->hb_font = hb_font_create(hb_face);
  hb_face_destroy(hb_face);
  hb_ot_font_set_funcs(face->hb_font);

  face->units_per_em = face->ft_face->units_per_EM;
  face->ascender = face->ft_face->ascender;
  face->descender = -face->ft_face->descender;
  face->line_gap = face->ft_face->height - face->ascender - face->descender;
  if (face->line_gap < 0) {
    face->line_gap = 0;
  }
  faces_.push_back(std::move(face));
  return static_cast<FaceId>(faces_.size() - 1);
}

auto FontSet::AddFaceFromFile(const std::string &path)
    -> std::expected<FaceId, std::string> {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected("cannot open " + path);
  }
  const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
  auto face = AddFace(bytes, false);
  if (!face) {
    return std::unexpected(face.error() + ": " + path);
  }
  return face;
}

auto FontSet::AddFont(std::vector<FaceId> chain) -> FontId {
  fonts_.push_back(std::move(chain));
  return static_cast<FontId>(fonts_.size() - 1);
}

auto FontSet::DefaultFallbackFiles() -> std::vector<std::string> {
  return {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
  };
}

auto FontSet::AddDefaultFonts(const std::vector<std::string> &fallback_files)
    -> DefaultFonts {
  const FaceId sans = *AddFace(kLiberationSansRegular, true);
  const FaceId bold = *AddFace(kLiberationSansBold, true);
  const FaceId mono = *AddFace(kLiberationMonoRegular, true);
  std::vector<FaceId> fallbacks;
  for (const std::string &path : fallback_files) {
    if (std::filesystem::exists(path)) {
      if (auto face = AddFaceFromFile(path)) {
        fallbacks.push_back(*face);
      }
    }
  }
  auto chain = [&](FaceId first) {
    std::vector<FaceId> c{first};
    c.insert(c.end(), fallbacks.begin(), fallbacks.end());
    return c;
  };
  DefaultFonts fonts;
  fonts.sans = AddFont(chain(sans));
  fonts.bold = AddFont(chain(bold));
  fonts.mono = AddFont(chain(mono));
  return fonts;
}

auto FontSet::Metrics(FontId font, float size) const -> FontMetrics {
  const Face &face = *faces_[fonts_[font].front()];
  const float scale = size / static_cast<float>(face.units_per_em);
  return {static_cast<float>(face.ascender) * scale,
          static_cast<float>(face.descender) * scale,
          static_cast<float>(face.line_gap) * scale};
}

auto FontSet::GlyphIndex(FaceId face, char32_t cp) const -> std::uint32_t {
  hb_codepoint_t glyph = 0;
  if (hb_font_get_nominal_glyph(faces_[face]->hb_font, cp, &glyph) == 0) {
    return 0;
  }
  return glyph;
}

}  // namespace gloomy::text
