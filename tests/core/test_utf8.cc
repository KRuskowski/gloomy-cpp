#include "gloomy/core/utf8.h"

#include <gtest/gtest.h>

namespace gloomy {
namespace {

TEST(Utf8Test, DecodesAllLengths) {
  const std::string text = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
  std::size_t pos = 0;
  EXPECT_EQ(DecodeUtf8(text, pos), U'a');
  EXPECT_EQ(DecodeUtf8(text, pos), U'é');
  EXPECT_EQ(DecodeUtf8(text, pos), U'€');
  EXPECT_EQ(DecodeUtf8(text, pos), U'\U0001F600');
  EXPECT_EQ(pos, text.size());
}

TEST(Utf8Test, MalformedInputAdvancesOneByte) {
  const std::string text = "\xC3(\xFF";
  std::size_t pos = 0;
  EXPECT_EQ(DecodeUtf8(text, pos), kReplacementChar);
  EXPECT_EQ(pos, 1U);
  EXPECT_EQ(DecodeUtf8(text, pos), U'(');
  EXPECT_EQ(DecodeUtf8(text, pos), kReplacementChar);
  EXPECT_EQ(pos, 3U);
}

TEST(Utf8Test, EncodeRoundTrips) {
  for (char32_t cp : {U'x', U'ü', U'中', U'\U0001F680'}) {
    std::string s;
    AppendUtf8(cp, s);
    EXPECT_EQ(static_cast<int>(s.size()), Utf8Length(cp));
    std::size_t pos = 0;
    EXPECT_EQ(DecodeUtf8(s, pos), cp);
  }
}

TEST(Utf8Test, PrevSkipsContinuationBytes) {
  const std::string text = "a\xE2\x82\xAC";
  EXPECT_EQ(PrevUtf8(text, 4), 1U);
  EXPECT_EQ(PrevUtf8(text, 1), 0U);
  EXPECT_EQ(PrevUtf8(text, 0), 0U);
}

TEST(Utf8Test, FoldCaseKeepsByteLength) {
  const std::string text = "Hello \xC3\x96L \xCE\xA9 \xD0\x96 \xC4\xB0";
  std::string folded;
  FoldCaseUtf8(text, folded);
  EXPECT_EQ(folded.size(), text.size());
  EXPECT_EQ(folded, "hello \xC3\xB6l \xCF\x89 \xD0\xB6 \xC4\xB0");
}

}  // namespace
}  // namespace gloomy
