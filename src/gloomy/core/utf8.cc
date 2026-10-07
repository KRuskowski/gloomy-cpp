#include "gloomy/core/utf8.h"

namespace gloomy {

auto DecodeUtf8(std::string_view text, std::size_t &pos) -> char32_t {
  const auto b0 = static_cast<unsigned char>(text[pos]);
  if (b0 < 0x80) {
    ++pos;
    return b0;
  }
  int length = 0;
  char32_t cp = 0;
  char32_t min = 0;
  if ((b0 & 0xE0U) == 0xC0U) {
    length = 2;
    cp = b0 & 0x1FU;
    min = 0x80;
  } else if ((b0 & 0xF0U) == 0xE0U) {
    length = 3;
    cp = b0 & 0x0FU;
    min = 0x800;
  } else if ((b0 & 0xF8U) == 0xF0U) {
    length = 4;
    cp = b0 & 0x07U;
    min = 0x10000;
  } else {
    ++pos;
    return kReplacementChar;
  }
  if (pos + length > text.size()) {
    ++pos;
    return kReplacementChar;
  }
  for (int i = 1; i < length; ++i) {
    const auto b = static_cast<unsigned char>(text[pos + i]);
    if ((b & 0xC0U) != 0x80U) {
      ++pos;
      return kReplacementChar;
    }
    cp = (cp << 6U) | (b & 0x3FU);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    ++pos;
    return kReplacementChar;
  }
  pos += length;
  return cp;
}

auto Utf8Length(char32_t cp) -> int {
  if (cp < 0x80) {
    return 1;
  }
  if (cp < 0x800) {
    return 2;
  }
  if (cp < 0x10000) {
    return 3;
  }
  return 4;
}

void AppendUtf8(char32_t cp, std::string &out) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  } else {
    out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  }
}

auto PrevUtf8(std::string_view text, std::size_t pos) -> std::size_t {
  if (pos == 0) {
    return 0;
  }
  --pos;
  while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0U) == 0x80U) {
    --pos;
  }
  return pos;
}

auto FoldCase(char32_t cp) -> char32_t {
  if (cp < 0x80) {
    return (cp >= 'A' && cp <= 'Z') ? cp + 32 : cp;
  }
  // Latin-1: A-grave .. Thorn, except the multiplication sign.
  if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) {
    return cp + 32;
  }
  // Latin Extended-A: mostly pairs, upper case even.
  if (cp >= 0x100 && cp <= 0x137) {
    // Dotted capital I folds to ASCII i, which is shorter; keep it.
    return cp == 0x130 ? cp : (cp | 1U);
  }
  if (cp >= 0x139 && cp <= 0x148) {
    return (cp & 1U) != 0 ? cp + 1 : cp;
  }
  if (cp >= 0x14A && cp <= 0x177) {
    return cp | 1U;
  }
  if (cp == 0x178) {
    return 0xFF;
  }
  if (cp >= 0x179 && cp <= 0x17E) {
    return (cp & 1U) != 0 ? cp + 1 : cp;
  }
  // Greek capitals.
  if ((cp >= 0x391 && cp <= 0x3A1) || (cp >= 0x3A3 && cp <= 0x3AB)) {
    return cp + 32;
  }
  // Cyrillic.
  if (cp >= 0x410 && cp <= 0x42F) {
    return cp + 32;
  }
  if (cp >= 0x400 && cp <= 0x40F) {
    return cp + 80;
  }
  return cp;
}

void FoldCaseUtf8(std::string_view text, std::string &out) {
  out.clear();
  out.reserve(text.size());
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t start = pos;
    const char32_t cp = DecodeUtf8(text, pos);
    const char32_t folded = FoldCase(cp);
    if (folded == cp || cp == kReplacementChar ||
        Utf8Length(folded) != static_cast<int>(pos - start)) {
      out.append(text.substr(start, pos - start));
    } else {
      AppendUtf8(folded, out);
    }
  }
}

}  // namespace gloomy
