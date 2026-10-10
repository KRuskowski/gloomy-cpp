#ifndef GLOOMY_UI_PAPER_H_
#define GLOOMY_UI_PAPER_H_

#include <cstdint>
#include <span>
#include <string_view>

#include "gloomy/icon/icon_set.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/skin.h"
#include "gloomy/ui/summary.h"

namespace gloomy::ui {

// Stylised documents, answer first: a contract, a certificate or an
// insurance policy drawn as the thing it is, so that it is recognised at a
// glance (who issued it, what it is worth, where it stands), with the
// numbers as big figures and the fine print folded below. Tables behind
// them stay the host's (tabs, grids). Drawn in a PanelSkin
// (gloomy/ui/skin.h); docs/design.md section 13.3.

/// Who issued a document: name, a line under it (faction, record), the
/// house colour and an icon.
struct Letterhead {
  std::string_view name;
  std::string_view line;
  /// The house colour: the letterhead strip and the seal; alpha 0 uses
  /// the skin's ink.
  Color color{0, 0, 0, 0};
  icon::IconId icon = icon::kNoIcon;
};

/// A key figure: "Pays" "+2,400 Cr", big; `why` is its tooltip.
struct SheetFigure {
  std::string_view label;
  std::string_view value;
  Tone tone = Tone::kNeutral;
  std::string_view why;
};

/// A party's signature line: role ("Issuer", "Contractor"), name, its
/// colour (alpha 0: the skin's text), whether it has signed and when.
struct Signatory {
  std::string_view role;
  std::string_view name;
  Color color{0, 0, 0, 0};
  bool signed_ = false;
  std::string_view date;
};

/// A rubber stamp for a document's state ("OPEN", "AWARDED", "FULFILLED",
/// "BREACHED"); empty text draws none.
struct Stamp {
  std::string_view text;
  Tone tone = Tone::kNeutral;
};

/// A contract as a document.
struct ContractSheetSpec {
  Letterhead issuer;
  /// The kind in small capitals over the title: "Supply contract".
  std::string_view kind;
  std::string_view title;
  /// Reference and date line: "No. 42 - posted day 3 08:00".
  std::string_view reference;
  /// Two to four figures that decide it.
  std::span<const SheetFigure> figures;
  Stamp stamp;
  /// Signature lines at the foot.
  std::span<const Signatory> parties;
  /// The fine print, folded under "Clauses (N)" until opened.
  std::span<const std::string_view> clauses;
  /// The skin; null derives one from the theme (PanelSkin::FromTheme()).
  const PanelSkin *skin = nullptr;
};

/// What the user did with a sheet this frame.
struct SheetResult {
  /// The clauses are unfolded (after this frame's click).
  bool clauses_open = false;
  /// The clauses toggle was clicked.
  bool clauses_toggled = false;
  /// Index of the figure clicked; -1 for none.
  int figure = -1;
};

/// Opens a contract sheet: letterhead, kind and title, the stamp, the
/// figures. The host adds its own content (chips, the primary action) as
/// children, then calls EndContractSheet() with the same spec.
/// @return The sheet's node.
auto BeginContractSheet(Context &ctx, std::string_view label,
                        const ContractSheetSpec &spec) -> int;

/// Closes a contract sheet: the signature lines and the folded clauses.
auto EndContractSheet(Context &ctx, const ContractSheetSpec &spec)
    -> SheetResult;

/// A bond or share certificate.
struct CertificateSpec {
  Letterhead issuer;
  /// "War bond".
  std::string_view title;
  /// The face value, big: "1,000 Cr"; and its label ("Face value").
  std::string_view face;
  std::string_view face_label = "Face value";
  /// Terms under the face: price, coupon, maturity.
  std::span<const SheetFigure> terms;
  /// The line that says what happens if things go wrong: "If Argon loses:
  /// 500 Cr back".
  std::string_view risk;
  Tone risk_tone = Tone::kWarning;
  /// Serial and date line in small type.
  std::string_view serial;
  /// How many the holder has: drawn as a stack of sheets behind (up to
  /// four) and a count; 0 or 1 draws one sheet.
  int held = 0;
  Stamp stamp;
  /// Picks the ornate border (OrnamentOf()): the issuer's id, so each
  /// issuer's certificates look alike.
  std::uint32_t ornament_seed = 0;
  /// Null: the theme's skin printed on paper (PanelSkin::OnPaper()).
  const PanelSkin *skin = nullptr;
};

/// The pixels a stack of `held` certificates adds right and below.
[[nodiscard]] auto CertificateStackOffset(int held) -> float;

/// A certificate: ornate border in the issuer's style, the issuer and
/// title, the face value big, the terms, the risk line, a seal, the
/// stamp, and a stack of sheets behind for holdings.
/// @return True if it was clicked.
auto Certificate(Context &ctx, std::string_view label,
                 const CertificateSpec &spec) -> bool;

/// An insurance policy as a document.
struct PolicySpec {
  Letterhead insurer;
  /// "Cargo policy", "Hull policy".
  std::string_view title;
  /// Policy number and term line.
  std::string_view number;
  /// What is insured: "Courier 'Lunar Wind' and its cargo".
  std::string_view insured;
  /// Cover, premium, deductible, term.
  std::span<const SheetFigure> figures;
  /// What it does not pay for: "War zones (Argon - Paranid front)",
  /// "Contraband".
  std::span<const std::string_view> exclusions;
  Stamp stamp;
  std::span<const Signatory> parties;
  /// Null: the theme's skin printed on paper.
  const PanelSkin *skin = nullptr;
};

/// An insurance policy: letterhead, title, the insured asset, the
/// figures, the exclusions boxed and hatched as not covered, the stamp
/// and the signatures.
/// @return True if it was clicked.
auto PolicySheet(Context &ctx, std::string_view label, const PolicySpec &spec)
    -> bool;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_PAPER_H_
