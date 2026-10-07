#ifndef GLOOMY_UI_SUMMARY_H_
#define GLOOMY_UI_SUMMARY_H_

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"

namespace gloomy::ui {

// Answer-first and progressive-disclosure widgets (docs/design.md
// section 2): a value with its reason, a glance card that drills down,
// a one-line answer with one action.

/// A value with a short "why".
struct ExplainedValue {
  std::string value;
  /// One line; shown as a tooltip (or inline when asked to).
  std::string why;
};

/// Semantic tone of a value.
enum class Tone : std::uint8_t { kNeutral, kGood, kWarning, kBad };

/// Shows `value`; its explanation appears as a tooltip on hover or focus,
/// or below it when `inline_note`. A dotted underline marks explained
/// values.
void ExplainedLabel(Context &ctx, std::string_view label,
                    const ExplainedValue &value, Tone tone = Tone::kNeutral,
                    bool inline_note = false);

/// A small line chart of `values` (oldest first), last point marked.
void Sparkline(Context &ctx, std::span<const float> values,
               SizeSpec width = SizeSpec::Fixed(96.0F), float height = 24.0F,
               Tone tone = Tone::kNeutral);

/// What a SummaryCard shows at a glance.
struct SummarySpec {
  std::string_view title;
  ExplainedValue value;
  /// e.g. "+2.1% today"; empty for none.
  std::string_view delta;
  Tone delta_tone = Tone::kNeutral;
  std::span<const float> sparkline;
  /// Colours the card's left edge.
  Tone status = Tone::kNeutral;
};

/// A glance card: title, headline value (explained), delta, sparkline.
/// Click or Enter drills down to the next level.
/// @return True when the user drills down.
auto SummaryCard(Context &ctx, std::string_view label, const SummarySpec &spec)
    -> bool;

/// One line answering the player's question, its reason, and exactly one
/// action. @return True when the action is taken (click, Enter).
auto AnswerLine(Context &ctx, std::string_view label, std::string_view answer,
                std::string_view reason, std::string_view action) -> bool;

/// Colour of a tone in the current theme (text colour for neutral).
auto ToneColor(const Theme &theme, Tone tone) -> Color;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_SUMMARY_H_
