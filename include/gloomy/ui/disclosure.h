#ifndef GLOOMY_UI_DISCLOSURE_H_
#define GLOOMY_UI_DISCLOSURE_H_

#include <map>
#include <span>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/summary.h"

namespace gloomy::ui {

// Progressive disclosure (docs/design.md section 2): a card that leads
// with the headline, the key facts that decide it and one big action;
// chips for a status at a glance; tabs that remember the last choice per
// kind of item; a segmented control for views; section headers and form
// rows that keep panels and dialogs consistent.

struct CardOptions {
  /// Colours the left edge; neutral draws the theme's border colour.
  Tone status = Tone::kNeutral;
  /// Alpha > 0: the left edge in this colour instead of the tone's.
  Color accent{0, 0, 0, 0};
  /// Padding inside the card.
  float padding = 12.0F;
  /// Space between the card's rows.
  float gap = 6.0F;
};

/// Opens a card: a raised panel with a coloured left edge whose children
/// stack in a column stretched to its width. EndCard() closes it.
/// @return The card's node index.
auto BeginCard(Context &ctx, std::string_view label,
               const CardOptions &opt = {}) -> int;

void EndCard(Context &ctx);

/// A row of key facts, each a small muted label over its value (coloured
/// by its tone, explained in a tooltip when it has a "why"), left to
/// right; long values are cut with an ellipsis when space is short.
void KeyFacts(Context &ctx, std::string_view label,
              std::span<const KeyNumber> facts);

/// A small pill with `text`, tinted by its tone (or `color` when its alpha
/// is above 0): a status at a glance.
void Chip(Context &ctx, std::string_view text, Tone tone = Tone::kNeutral,
          Color color = {0, 0, 0, 0});

/// A section header: a small bold title in the accent colour, an optional
/// muted aside at the right, and a divider under both.
void SectionHeader(Context &ctx, std::string_view title,
                   std::string_view aside = {});

/// Opens a form row: `label` in a fixed-width column, then the row's
/// controls until EndFormRow(), so that the inputs of a form line up.
void BeginFormRow(Context &ctx, std::string_view label,
                  float label_width = 120.0F);

void EndFormRow(Context &ctx);

struct BigButtonOptions {
  /// A second, smaller line under the label (what happens, what it costs).
  std::string_view caption;
  /// The action cannot be taken now: drawn greyed out, never activates.
  bool disabled = false;
  /// Why it is disabled, in a tooltip and as the caption.
  std::string_view reason;
  /// False: drawn in the surface colour rather than the primary one, for
  /// an action that can be taken but is not advised.
  bool emphasis = true;
  SizeSpec width = SizeSpec::Fit(160.0F);
};

/// The one primary action of a view: a large button with an optional
/// caption. Enter or Space while focused activates it.
/// @return True when activated.
auto BigButton(Context &ctx, std::string_view label,
               const BigButtonOptions &opt = {}) -> bool;

/// What an empty view says: why nothing is here and the one way on.
struct EmptyStateSpec {
  std::string_view title;
  /// Why it is empty, or what will fill it.
  std::string_view hint;
  /// The one action that helps; empty for none.
  std::string_view action;
};

/// A centred note for an empty list or pane: title, hint and an optional
/// action button, so an empty view still answers "what now?".
/// @return True when the action is taken.
auto EmptyState(Context &ctx, std::string_view label,
                const EmptyStateSpec &spec) -> bool;

/// One segment of a SegmentedControl.
struct Segment {
  std::string_view text;
  /// Shown after the text in a smaller font; negative for none.
  int count = -1;
  /// Shown on hover.
  std::string_view tooltip;
};

/// A row of joined toggle segments, exactly one selected (or none when
/// `selected` is out of range). Left/Right move while it has focus.
/// Segments with a count of 0 are dimmed.
/// @return True when the selection changed.
auto SegmentedControl(Context &ctx, std::string_view label, int &selected,
                      std::span<const Segment> segments) -> bool;

/// The tab last chosen in each group of tab bars (say, per kind of item a
/// detail pane shows), remembered by tab name: a group whose tabs vary
/// keeps its choice while the tab exists and falls back to the first tab
/// (the overview) while it does not.
class TabMemory {
 public:
  /// Index of `group`'s remembered tab in `labels`; 0 if none is
  /// remembered or it is not among them.
  [[nodiscard]] auto Get(std::string_view group,
                         std::span<const std::string> labels) const -> int;

  void Set(std::string_view group, std::string_view tab);

  /// Forgets every choice.
  void Clear() { chosen_.clear(); }

 private:
  std::map<std::string, std::string, std::less<>> chosen_;
};

/// A tab bar (Tabs()) whose choice is kept in `memory` under `group`.
/// @return The selected tab's index.
auto RememberedTabs(Context &ctx, std::string_view label, TabMemory &memory,
                    std::string_view group, std::span<const std::string> labels)
    -> int;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_DISCLOSURE_H_
