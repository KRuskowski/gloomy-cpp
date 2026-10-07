#ifndef GLOOMY_DATA_DOCUMENT_H_
#define GLOOMY_DATA_DOCUMENT_H_

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/data/search.h"

namespace gloomy::data {

/// Kinds of document blocks.
enum class BlockKind : std::uint8_t {
  kHeading,
  kParagraph,
  kListItem,
  kQuote,
  kCode,
  kRule,
};

/// Inline style bits of a span.
enum SpanStyle : std::uint8_t {
  kSpanBold = 1U << 0U,
  kSpanItalic = 1U << 1U,
  kSpanMono = 1U << 2U,
  kSpanLink = 1U << 3U,
  kSpanHighlight = 1U << 4U,
};

/// A styled byte range of a block's text.
struct DocSpan {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  std::uint8_t style = 0;
  /// 0xRRGGBBAA, or 0 for the default colour.
  std::uint32_t color = 0;
  /// Link target (e.g. "station:42") when style has kSpanLink.
  std::string link;

  friend auto operator==(const DocSpan &, const DocSpan &) -> bool = default;
};

/// A block of a document: plain text plus styled spans.
struct DocBlock {
  BlockKind kind = BlockKind::kParagraph;
  /// Heading level (1-3) or list depth (0-based).
  int level = 0;
  std::string text;
  std::vector<DocSpan> spans;
};

/// A section heading, for outlines and navigation.
struct DocSection {
  std::uint32_t block = 0;
  int level = 1;
  std::string title;
};

/// A search match inside a document.
struct DocMatch {
  std::uint32_t block = 0;
  MatchRange range;
};

/// A long structured text such as a contract: blocks with inline styles
/// and links, searchable through the shared search core
/// (docs/design.md section 12).
class Document {
 public:
  /// Parses a Markdown subset: "#".."###" headings, "-"/"*"/"1." list items
  /// (two spaces of indent per level), "> " quotes, ``` code blocks, "---"
  /// rules, paragraphs separated by blank lines; inline **bold**,
  /// *italic*, `mono`, [text](target) links, <hl>highlight</hl> and
  /// <color=#rrggbb>text</color>. Backslash escapes a character.
  static auto ParseMarkdown(std::string_view source) -> Document;

  /// Parses inline markup into `block`'s text and spans.
  static void ParseInline(std::string_view source, DocBlock &block);

  void AddBlock(DocBlock block);

  auto blocks() const -> std::span<const DocBlock> { return blocks_; }

  auto size() const -> std::uint32_t {
    return static_cast<std::uint32_t>(blocks_.size());
  }

  /// Headings in order.
  auto Outline() const -> std::vector<DocSection>;

  /// The search corpus: one corpus document per block. Built on first use.
  auto corpus() const -> const SearchCorpus &;

  /// All matches of `terms` (folded), in document order.
  auto FindAll(std::span<const std::string> terms) const
      -> std::vector<DocMatch>;

  /// Changes whenever blocks are added.
  auto version() const -> std::uint64_t { return version_; }

 private:
  std::vector<DocBlock> blocks_;
  std::uint64_t version_ = 1;
  mutable std::shared_ptr<SearchCorpus> corpus_;
  mutable std::uint64_t corpus_version_ = 0;
};

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_DOCUMENT_H_
