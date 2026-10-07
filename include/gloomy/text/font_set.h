#ifndef GLOOMY_TEXT_FONT_SET_H_
#define GLOOMY_TEXT_FONT_SET_H_

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

// Opaque library types, so users of this header need neither FreeType nor
// HarfBuzz headers.
struct FT_FaceRec_;
struct hb_font_t;

namespace gloomy::text {

/// One loaded font file (face).
using FaceId = std::uint16_t;

/// A font: a fallback chain of faces, tried in order per code point.
using FontId = std::uint16_t;

/// Vertical metrics of a font at a pixel size. Descent is positive.
struct FontMetrics {
  float ascent = 0.0F;
  float descent = 0.0F;
  float line_gap = 0.0F;

  auto LineHeight() const -> float { return ascent + descent + line_gap; }
};

/// The fonts registered by AddDefaultFonts().
struct DefaultFonts {
  FontId sans = 0;
  FontId bold = 0;
  FontId italic = 0;
  FontId mono = 0;
};

/// Owns FreeType, the loaded faces (each also as a HarfBuzz font) and the
/// fallback chains. See docs/design.md section 7.
class FontSet {
 public:
  FontSet();
  FontSet(const FontSet &) = delete;
  auto operator=(const FontSet &) -> FontSet & = delete;
  ~FontSet();

  /// Loads a face from memory. The bytes are copied unless `is_static`,
  /// in which case they must outlive the FontSet (embedded fonts).
  auto AddFace(std::span<const unsigned char> data, bool is_static = false)
      -> std::expected<FaceId, std::string>;

  /// Loads a face from a file.
  auto AddFaceFromFile(const std::string &path)
      -> std::expected<FaceId, std::string>;

  /// Defines a font as a fallback chain of faces; the first face's metrics
  /// are the font's metrics.
  auto AddFont(std::vector<FaceId> chain) -> FontId;

  /// Registers the embedded Liberation Sans (regular, bold, italic) and
  /// Liberation Mono, each followed by those `fallback_files` that exist
  /// (for symbols and scripts Liberation lacks).
  auto AddDefaultFonts(const std::vector<std::string> &fallback_files =
                           DefaultFallbackFiles()) -> DefaultFonts;

  /// System fonts tried as fallbacks by default (DejaVu Sans, Noto CJK).
  static auto DefaultFallbackFiles() -> std::vector<std::string>;

  auto Chain(FontId font) const -> std::span<const FaceId> {
    return fonts_[font];
  }

  auto Metrics(FontId font, float size) const -> FontMetrics;

  auto ft_face(FaceId face) const -> FT_FaceRec_ * {
    return faces_[face]->ft_face;
  }

  auto hb_font(FaceId face) const -> hb_font_t * {
    return faces_[face]->hb_font;
  }

  /// Glyph index of `cp` in `face`, 0 if missing.
  auto GlyphIndex(FaceId face, char32_t cp) const -> std::uint32_t;

  auto face_count() const -> int { return static_cast<int>(faces_.size()); }

  auto font_count() const -> int { return static_cast<int>(fonts_.size()); }

 private:
  struct Face {
    std::vector<unsigned char> owned;
    FT_FaceRec_ *ft_face = nullptr;
    hb_font_t *hb_font = nullptr;
    int units_per_em = 1000;
    int ascender = 0;
    int descender = 0;
    int line_gap = 0;
  };

  void *library_ = nullptr;
  std::vector<std::unique_ptr<Face>> faces_;
  std::vector<std::vector<FaceId>> fonts_;
};

}  // namespace gloomy::text

#endif  // GLOOMY_TEXT_FONT_SET_H_
