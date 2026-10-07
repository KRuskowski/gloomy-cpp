#ifndef GLOOMY_CORE_ID_H_
#define GLOOMY_CORE_ID_H_

#include <cstdint>
#include <string_view>

namespace gloomy {

/// Identifies a UI node across frames. Hashed from the id stack.
using Id = std::uint64_t;

/// The id of nothing.
inline constexpr Id kNoId = 0;

/// Mixes a 64-bit value into `seed` (splitmix64 finaliser).
constexpr auto HashCombine(std::uint64_t seed, std::uint64_t value)
    -> std::uint64_t {
  std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL + value;
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

/// FNV-1a over bytes, then mixed into `seed`.
constexpr auto HashString(std::string_view text, std::uint64_t seed = 0)
    -> std::uint64_t {
  std::uint64_t h = 0xCBF29CE484222325ULL;
  for (const char c : text) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001B3ULL;
  }
  return HashCombine(seed, h);
}

}  // namespace gloomy

#endif  // GLOOMY_CORE_ID_H_
