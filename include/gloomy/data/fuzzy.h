#ifndef GLOOMY_DATA_FUZZY_H_
#define GLOOMY_DATA_FUZZY_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace gloomy::data {

/// A fuzzy match: higher scores are better; positions are the byte
/// offsets of the matched characters in the text.
struct FuzzyMatch {
  int score = 0;
  std::vector<std::uint32_t> positions;
};

/// fzf-style subsequence matching for command names and short labels
/// (not for data cells): every pattern character must appear in order,
/// case-insensitively. Matches at word starts, consecutive runs and the
/// start of the text score higher; gaps cost a little.
/// @return std::nullopt if `pattern` is not a subsequence of `text`.
auto FuzzyMatchText(std::string_view pattern, std::string_view text)
    -> std::optional<FuzzyMatch>;

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_FUZZY_H_
