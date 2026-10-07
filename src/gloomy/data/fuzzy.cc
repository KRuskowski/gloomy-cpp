#include "gloomy/data/fuzzy.h"

#include <algorithm>
#include <climits>

#include "gloomy/core/utf8.h"

namespace gloomy::data {

namespace {

constexpr int kMatch = 16;
constexpr int kWordStart = 24;
constexpr int kConsecutive = 18;
// A gap between matched characters costs kGapStart plus kGapExtend per
// further skipped character; characters before the first match cost a
// quarter point each.
constexpr int kGapStart = 3;
constexpr int kGapExtend = 1;
constexpr int kNone = INT_MIN / 4;

auto IsSeparator(char c) -> bool {
  return c == ' ' || c == '_' || c == '-' || c == '.' || c == '/' || c == ':' ||
         c == '(' || c == ',';
}

}  // namespace

auto FuzzyMatchText(std::string_view pattern, std::string_view text)
    -> std::optional<FuzzyMatch> {
  std::string p;
  std::string t;
  FoldCaseUtf8(pattern, p);
  FoldCaseUtf8(text, t);
  // Drop spaces from the pattern: "sh pa" matches "Ship Parts".
  std::erase(p, ' ');
  if (p.empty()) {
    return FuzzyMatch{};
  }
  const std::size_t m = p.size();
  const std::size_t n = t.size();
  if (m > n || n > 512) {
    return std::nullopt;
  }
  // Quick subsequence check.
  std::size_t k = 0;
  for (std::size_t j = 0; j < n && k < m; ++j) {
    if (t[j] == p[k]) {
      ++k;
    }
  }
  if (k < m) {
    return std::nullopt;
  }
  // best[i][j]: best score with pattern[i] matched at text[j]; run[i][j]:
  // the word-start bonus of the consecutive run ending there, which every
  // character of the run inherits (fzf).
  std::vector<int> best(m * n, kNone);
  std::vector<int> run_bonus(m * n, 0);
  std::vector<int> from(m * n, -1);
  auto bonus = [&](std::size_t j) {
    if (j == 0 || IsSeparator(text[j - 1]) ||
        (text[j - 1] >= 'a' && text[j - 1] <= 'z' && text[j] >= 'A' &&
         text[j] <= 'Z')) {
      return kWordStart;
    }
    return 0;
  };
  for (std::size_t j = 0; j < n; ++j) {
    if (t[j] == p[0]) {
      best[j] = kMatch + bonus(j) - static_cast<int>(j) / 4;
      run_bonus[j] = bonus(j);
    }
  }
  for (std::size_t i = 1; i < m; ++i) {
    // Best previous-row score ending at least two characters back, minus
    // the gap extension cost.
    int run = kNone;
    int run_from = -1;
    for (std::size_t j = 1; j < n; ++j) {
      if (run != kNone) {
        run -= kGapExtend;
      }
      if (j >= 2) {
        const int cand = best[(i - 1) * n + j - 2];
        if (cand > run) {
          run = cand;
          run_from = static_cast<int>(j - 2);
        }
      }
      if (t[j] != p[i]) {
        continue;
      }
      int score = kNone;
      int origin = -1;
      int rb = bonus(j);
      if (run != kNone) {
        score = run - kGapStart + kMatch + bonus(j);
        origin = run_from;
      }
      const int prev = best[(i - 1) * n + j - 1];
      if (prev != kNone) {
        const int inherited =
            std::max(bonus(j), run_bonus[(i - 1) * n + j - 1]);
        const int consecutive = prev + kMatch + inherited + kConsecutive;
        if (consecutive > score) {
          score = consecutive;
          origin = static_cast<int>(j - 1);
          rb = inherited;
        }
      }
      best[i * n + j] = score;
      run_bonus[i * n + j] = rb;
      from[i * n + j] = origin;
    }
  }
  int top = kNone;
  int at = -1;
  for (std::size_t j = 0; j < n; ++j) {
    if (best[(m - 1) * n + j] > top) {
      top = best[(m - 1) * n + j];
      at = static_cast<int>(j);
    }
  }
  if (at < 0) {
    return std::nullopt;
  }
  FuzzyMatch result;
  result.score = top;
  result.positions.resize(m);
  for (std::size_t i = m; i-- > 0;) {
    result.positions[i] = static_cast<std::uint32_t>(at);
    at = from[i * n + static_cast<std::size_t>(at)];
  }
  return result;
}

}  // namespace gloomy::data
