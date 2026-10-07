#include "gloomy/data/document.h"

#include <algorithm>
#include <charconv>

namespace gloomy::data {

namespace {

// Per-byte style while parsing inline markup.
struct Attr {
  std::uint8_t style = 0;
  std::uint32_t color = 0;
  int link = -1;

  friend auto operator==(const Attr &, const Attr &) -> bool = default;
};

auto TrimLeft(std::string_view s, std::size_t &indent) -> std::string_view {
  indent = 0;
  while (indent < s.size() && s[indent] == ' ') {
    ++indent;
  }
  return s.substr(indent);
}

auto ParseColor(std::string_view hex) -> std::uint32_t {
  if (hex.size() != 6 && hex.size() != 8) {
    return 0;
  }
  std::uint32_t v = 0;
  const auto r = std::from_chars(hex.data(), hex.data() + hex.size(), v, 16);
  if (r.ec != std::errc()) {
    return 0;
  }
  return hex.size() == 6 ? (v << 8U) | 0xFFU : v;
}

}  // namespace

void Document::ParseInline(std::string_view src, DocBlock &block) {
  std::string &out = block.text;
  out.clear();
  std::vector<Attr> attrs;
  std::vector<std::string> links;
  Attr cur;
  std::vector<std::uint32_t> colors;
  auto emit = [&](char c) {
    out.push_back(c);
    attrs.push_back(cur);
  };
  std::size_t i = 0;
  while (i < src.size()) {
    const std::string_view rest = src.substr(i);
    if (rest[0] == '\\' && rest.size() > 1) {
      emit(rest[1]);
      i += 2;
      continue;
    }
    if ((cur.style & kSpanMono) != 0) {
      if (rest[0] == '`') {
        cur.style &= static_cast<std::uint8_t>(~kSpanMono);
        ++i;
      } else {
        emit(rest[0]);
        ++i;
      }
      continue;
    }
    if (rest.starts_with("**")) {
      cur.style ^= kSpanBold;
      i += 2;
      continue;
    }
    if (rest[0] == '*') {
      cur.style ^= kSpanItalic;
      ++i;
      continue;
    }
    if (rest[0] == '`') {
      cur.style |= kSpanMono;
      ++i;
      continue;
    }
    if (rest.starts_with("<hl>")) {
      cur.style |= kSpanHighlight;
      i += 4;
      continue;
    }
    if (rest.starts_with("</hl>")) {
      cur.style &= static_cast<std::uint8_t>(~kSpanHighlight);
      i += 5;
      continue;
    }
    if (rest.starts_with("<color=#")) {
      const std::size_t close = rest.find('>');
      if (close != std::string_view::npos) {
        colors.push_back(cur.color);
        cur.color = ParseColor(rest.substr(8, close - 8));
        i += close + 1;
        continue;
      }
    }
    if (rest.starts_with("</color>")) {
      cur.color = colors.empty() ? 0 : colors.back();
      if (!colors.empty()) {
        colors.pop_back();
      }
      i += 8;
      continue;
    }
    if (rest[0] == '[') {
      const std::size_t mid = rest.find("](");
      const std::size_t close =
          mid == std::string_view::npos ? mid : rest.find(')', mid);
      if (close != std::string_view::npos) {
        links.emplace_back(rest.substr(mid + 2, close - mid - 2));
        const Attr saved = cur;
        cur.style |= kSpanLink;
        cur.link = static_cast<int>(links.size() - 1);
        for (const char c : rest.substr(1, mid - 1)) {
          emit(c);
        }
        cur = saved;
        i += close + 1;
        continue;
      }
    }
    emit(rest[0]);
    ++i;
  }
  // Compress per-byte attributes into spans of non-default style.
  block.spans.clear();
  std::size_t b = 0;
  while (b < attrs.size()) {
    std::size_t e = b;
    while (e < attrs.size() && attrs[e] == attrs[b]) {
      ++e;
    }
    const Attr &a = attrs[b];
    if (a.style != 0 || a.color != 0) {
      DocSpan span;
      span.begin = static_cast<std::uint32_t>(b);
      span.end = static_cast<std::uint32_t>(e);
      span.style = a.style;
      span.color = a.color;
      if (a.link >= 0) {
        span.link = links[static_cast<std::size_t>(a.link)];
      }
      block.spans.push_back(std::move(span));
    }
    b = e;
  }
}

auto Document::ParseMarkdown(std::string_view source) -> Document {
  Document doc;
  std::string paragraph;
  BlockKind para_kind = BlockKind::kParagraph;
  std::string code;
  bool in_code = false;
  auto flush = [&] {
    if (!paragraph.empty()) {
      DocBlock block;
      block.kind = para_kind;
      ParseInline(paragraph, block);
      doc.AddBlock(std::move(block));
      paragraph.clear();
    }
    para_kind = BlockKind::kParagraph;
  };
  std::size_t pos = 0;
  while (pos <= source.size()) {
    std::size_t nl = source.find('\n', pos);
    if (nl == std::string_view::npos) {
      nl = source.size();
    }
    std::string_view line = source.substr(pos, nl - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    pos = nl + 1;
    std::size_t indent = 0;
    const std::string_view trimmed = TrimLeft(line, indent);
    if (trimmed.starts_with("```")) {
      if (in_code) {
        DocBlock block;
        block.kind = BlockKind::kCode;
        if (!code.empty() && code.back() == '\n') {
          code.pop_back();
        }
        block.text = code;
        doc.AddBlock(std::move(block));
        code.clear();
      } else {
        flush();
      }
      in_code = !in_code;
      continue;
    }
    if (in_code) {
      code.append(line);
      code.push_back('\n');
      continue;
    }
    if (trimmed.empty()) {
      flush();
      continue;
    }
    if (trimmed == "---" || trimmed == "***") {
      flush();
      DocBlock block;
      block.kind = BlockKind::kRule;
      doc.AddBlock(std::move(block));
      continue;
    }
    int hashes = 0;
    while (hashes < static_cast<int>(trimmed.size()) &&
           trimmed[hashes] == '#') {
      ++hashes;
    }
    if (hashes >= 1 && hashes <= 3 &&
        hashes < static_cast<int>(trimmed.size()) && trimmed[hashes] == ' ') {
      flush();
      DocBlock block;
      block.kind = BlockKind::kHeading;
      block.level = hashes;
      ParseInline(trimmed.substr(static_cast<std::size_t>(hashes) + 1), block);
      doc.AddBlock(std::move(block));
      continue;
    }
    std::size_t marker = 0;
    if (trimmed.starts_with("- ") || trimmed.starts_with("* ")) {
      marker = 2;
    } else {
      std::size_t d = 0;
      while (d < trimmed.size() && trimmed[d] >= '0' && trimmed[d] <= '9') {
        ++d;
      }
      if (d > 0 && trimmed.substr(d).starts_with(". ")) {
        marker = d + 2;
      }
    }
    if (marker > 0) {
      flush();
      para_kind = BlockKind::kListItem;
      paragraph.assign(trimmed.substr(marker));
      // List items are one block per item; continuation lines append.
      DocBlock block;
      block.kind = BlockKind::kListItem;
      block.level = static_cast<int>(indent / 2);
      ParseInline(paragraph, block);
      // Keep numbering visible as text.
      if (marker > 2) {
        block.text.insert(0, std::string(trimmed.substr(0, marker)));
        for (DocSpan &s : block.spans) {
          s.begin += static_cast<std::uint32_t>(marker);
          s.end += static_cast<std::uint32_t>(marker);
        }
      }
      doc.AddBlock(std::move(block));
      paragraph.clear();
      para_kind = BlockKind::kParagraph;
      continue;
    }
    if (trimmed.starts_with("> ") || trimmed == ">") {
      if (para_kind != BlockKind::kQuote) {
        flush();
        para_kind = BlockKind::kQuote;
      }
      if (!paragraph.empty()) {
        paragraph.push_back(' ');
      }
      paragraph.append(
          trimmed.substr(std::min<std::size_t>(2, trimmed.size())));
      continue;
    }
    if (para_kind == BlockKind::kQuote) {
      flush();
    }
    if (!paragraph.empty()) {
      paragraph.push_back(' ');
    }
    paragraph.append(trimmed);
  }
  flush();
  if (in_code && !code.empty()) {
    DocBlock block;
    block.kind = BlockKind::kCode;
    block.text = code;
    doc.AddBlock(std::move(block));
  }
  return doc;
}

void Document::AddBlock(DocBlock block) {
  blocks_.push_back(std::move(block));
  ++version_;
}

auto Document::Outline() const -> std::vector<DocSection> {
  std::vector<DocSection> out;
  for (std::size_t i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].kind == BlockKind::kHeading) {
      out.push_back(
          {static_cast<std::uint32_t>(i), blocks_[i].level, blocks_[i].text});
    }
  }
  return out;
}

auto Document::corpus() const -> const SearchCorpus & {
  if (!corpus_ || corpus_version_ != version_) {
    corpus_ = std::make_shared<SearchCorpus>();
    for (const DocBlock &b : blocks_) {
      corpus_->Add(b.text);
    }
    corpus_version_ = version_;
  }
  return *corpus_;
}

auto Document::FindAll(std::span<const std::string> terms) const
    -> std::vector<DocMatch> {
  std::vector<DocMatch> out;
  if (terms.empty()) {
    return out;
  }
  const SearchCorpus &c = corpus();
  std::vector<MatchRange> ranges;
  for (std::uint32_t b = 0; b < c.size(); ++b) {
    // Within a document, any term counts (highlight every occurrence).
    const std::string_view text = c.Doc(b);
    const bool any =
        std::any_of(terms.begin(), terms.end(), [&](const std::string &t) {
          return text.find(t) != std::string::npos;
        });
    if (!any) {
      continue;
    }
    ranges.clear();
    FindMatches(blocks_[b].text, terms, ranges);
    for (const MatchRange &r : ranges) {
      out.push_back({b, r});
    }
  }
  return out;
}

}  // namespace gloomy::data
