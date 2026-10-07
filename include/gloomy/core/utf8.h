#ifndef GLOOMY_CORE_UTF8_H_
#define GLOOMY_CORE_UTF8_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace gloomy {

/// Code point substituted for malformed input.
inline constexpr char32_t kReplacementChar = 0xFFFD;

/// Decodes the code point starting at byte `pos` and advances `pos` past it.
/// Malformed sequences decode to kReplacementChar and advance one byte.
auto DecodeUtf8(std::string_view text, std::size_t &pos) -> char32_t;

/// Appends the UTF-8 encoding of `cp`.
void AppendUtf8(char32_t cp, std::string &out);

/// Number of bytes the UTF-8 encoding of `cp` takes.
auto Utf8Length(char32_t cp) -> int;

/// Byte offset of the code point before `pos` (0 if `pos` is 0).
auto PrevUtf8(std::string_view text, std::size_t pos) -> std::size_t;

/// Simple lower-case folding of one code point for case-insensitive search.
/// Covers ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic. Only maps
/// to code points with the same UTF-8 length, so folded text keeps the byte
/// offsets of the original.
auto FoldCase(char32_t cp) -> char32_t;

/// Folds `text` into `out` (cleared first); `out` has the same byte length
/// and the same code point boundaries as `text`.
void FoldCaseUtf8(std::string_view text, std::string &out);

}  // namespace gloomy

#endif  // GLOOMY_CORE_UTF8_H_
