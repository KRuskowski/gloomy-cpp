#include "gloomy/ui/paper.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

const std::vector<SheetFigure> kFigures = {
    {"Pays", "+2,400 Cr", Tone::kGood, "After crew and fees"},
    {"Due", "day 3 08:00", Tone::kNeutral, {}},
    {"Penalty", "1,200 Cr", Tone::kBad, {}},
};
const std::vector<Signatory> kParties = {
    {"Issuer", "Lunon Habitat", Color::Rgb(0x78B4FF), true, "day 2"},
    {"Contractor", "You", {}, false, {}},
};
const std::vector<std::string_view> kClauses = {
    "Deliver 400 Food to Lunon Habitat by day 3 08:00.",
    "Paid 42 Cr per unit on delivery from escrow.",
    "Undelivered units forfeit 20% of their price.",
};

auto Spec() -> ContractSheetSpec {
  ContractSheetSpec s;
  s.issuer = {
      "Lunon Habitat", "Argon Federation", Color::Rgb(0x78B4FF), icon::kNoIcon};
  s.kind = "Supply contract";
  s.title = "Deliver 400 Food to Lunon Habitat";
  s.reference = "No. 42 - posted day 2 10:00";
  s.figures = kFigures;
  s.stamp = {"Open", Tone::kGood};
  s.parties = kParties;
  s.clauses = kClauses;
  return s;
}

/// An id under the sheet's id.
auto SheetChild(Context &ctx, std::string_view sheet, std::string_view child)
    -> Id {
  ctx.PushId(ctx.GetId(sheet));
  const Id id = ctx.GetId(child);
  ctx.PopId();
  return id;
}

/// An id under a signature of the sheet (parties count from 1).
auto SignatureChild(Context &ctx, int party, std::string_view child) -> Id {
  ctx.PushId(ctx.GetId("##sheet"));
  ctx.PushId(static_cast<std::uint64_t>(party));
  const Id id = ctx.GetId(child);
  ctx.PopId();
  ctx.PopId();
  return id;
}

TEST(PaperTest, AnEmptyColourInitialiserMeansNone) {
  // `{}` must not be Color{}, which is opaque black.
  EXPECT_FALSE(kParties[1].color.has_value());
  const Letterhead head{"Teladi Mutual", "Underwriters", {}};
  EXPECT_FALSE(head.color.has_value());
}

TEST(PaperTest, SignaturesAndTheSealStandClearOfTheRule) {
  for (const float width : {700.0F, 420.0F}) {
    UiHarness h({width, 600});
    const ContractSheetSpec spec = Spec();
    auto ui = [&](Context &ctx) {
      BeginColumn(ctx, {.width = SizeSpec::Grow()});
      BeginContractSheet(ctx, "##sheet", spec);
      (void)EndContractSheet(ctx, spec);
      End(ctx);
    };
    h.Frame(ui);
    h.Frame(ui);
    Context &c = h.ctx();
    for (int party = 1; party <= 2; ++party) {
      const Rect line =
          c.LastRect(SignatureChild(c, party, "##sig_line")).value();
      const Rect rule =
          c.LastRect(SignatureChild(c, party, "##sig_rule")).value();
      EXPECT_GE(rule.y0 - line.y1, kSignatureRuleGap - 0.5F)
          << "width " << width << " party " << party;
      EXPECT_NEAR(line.Height(), kSignatureLineHeight, 0.5F);
      // The seal (drawn for the signed issuer) fits in the name line, above
      // the rule and inside the column.
      const Vec2 seal = SignatureSealCentre(rule);
      EXPECT_LT(seal.y + kSignatureSealRadius, rule.y0);
      EXPECT_GE(seal.y - kSignatureSealRadius, line.y0);
      EXPECT_LE(seal.x + kSignatureSealRadius, rule.x1);
    }
  }
}

TEST(PaperTest, ContractSheetHoldsTheHostsContentAndFoldsClauses) {
  UiHarness h({700, 600});
  const ContractSheetSpec spec = Spec();
  SheetResult result;
  bool pressed = false;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Grow()});
    BeginContractSheet(ctx, "##sheet", spec);
    pressed = Button(ctx, "Bid 42/unit", ButtonKind::kPrimary);
    result = EndContractSheet(ctx, spec);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  const Rect sheet = c.LastRect(c.GetId("##sheet")).value();
  const Id button = SheetChild(c, "##sheet", "Bid 42/unit");
  const Rect inside = c.LastRect(button).value();
  EXPECT_GE(inside.x0, sheet.x0);
  EXPECT_LE(inside.y1, sheet.y1);
  EXPECT_FALSE(result.clauses_open);
  // The host's button works inside the sheet.
  h.Frame(ui, UiHarness::Click(h.Center(button)));
  EXPECT_TRUE(pressed);
  // Unfolding the clauses makes the sheet taller.
  const float folded = sheet.Height();
  const Id toggle =
      HashCombine(HashCombine(c.GetId("##sheet"), 0x434CULL), 0x636CULL);
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(toggle)));
  EXPECT_TRUE(result.clauses_toggled);
  EXPECT_TRUE(result.clauses_open);
  h.Frame(ui);
  EXPECT_FALSE(result.clauses_toggled);
  EXPECT_TRUE(result.clauses_open) << "stays open";
  EXPECT_GT(c.LastRect(c.GetId("##sheet")).value().Height(), folded + 30.0F);
}

TEST(PaperTest, FiguresAreClickable) {
  UiHarness h({700, 600});
  const ContractSheetSpec spec = Spec();
  SheetResult result;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Grow()});
    BeginContractSheet(ctx, "##sheet", spec);
    result = EndContractSheet(ctx, spec);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  c.PushId(c.GetId("##sheet"));
  c.PushId("##figures");
  const Id penalty = c.GetId(std::uint64_t{3});
  const Id pays = c.GetId(std::uint64_t{1});
  c.PopId();
  c.PopId();
  // The figures share the row, in order.
  EXPECT_LT(c.LastRect(pays)->x0, c.LastRect(penalty)->x0);
  h.Frame(ui, UiHarness::Click(h.Center(penalty)));
  EXPECT_EQ(result.figure, 2);
  h.Frame(ui);
  EXPECT_EQ(result.figure, -1);
}

TEST(PaperTest, NestedSheetsCloseInOrder) {
  UiHarness h({700, 900});
  const ContractSheetSpec spec = Spec();
  int depth = 0;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Grow()});
    BeginContractSheet(ctx, "##outer", spec);
    BeginContractSheet(ctx, "##inner", spec);
    EndContractSheet(ctx, spec);
    EndContractSheet(ctx, spec);
    depth = ctx.current();
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  const Rect outer = c.LastRect(c.GetId("##outer")).value();
  c.PushId(c.GetId("##outer"));
  const Rect inner = c.LastRect(c.GetId("##inner")).value();
  c.PopId();
  EXPECT_GT(inner.x0, outer.x0);
  EXPECT_LT(inner.y1, outer.y1);
  EXPECT_GE(depth, 0);
}

TEST(PaperTest, CertificateStacksHoldings) {
  EXPECT_FLOAT_EQ(CertificateStackOffset(0), 0.0F);
  EXPECT_FLOAT_EQ(CertificateStackOffset(1), 0.0F);
  EXPECT_GT(CertificateStackOffset(3), CertificateStackOffset(2));
  EXPECT_FLOAT_EQ(CertificateStackOffset(4), CertificateStackOffset(400))
      << "at most four sheets show";
  UiHarness h({700, 500});
  const std::vector<SheetFigure> terms = {
      {"Price", "900 Cr", Tone::kNeutral, {}},
      {"Matures", "day 30", Tone::kNeutral, {}},
  };
  CertificateSpec spec;
  spec.issuer = {"Argon Federation", "War with Paranid Empire", {}};
  spec.title = "War bond";
  spec.face = "1,000 Cr";
  spec.terms = terms;
  spec.risk = "If Argon loses: 500 Cr back";
  spec.serial = "Series A - No. 0012";
  bool clicked = false;
  float single = 0.0F;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Fixed(400)});
    clicked = Certificate(ctx, "##bond", spec);
    End(ctx);
  };
  for (const int held : {1, 3}) {
    spec.held = held;
    h.Frame(ui);
    h.Frame(ui);
    const Rect r = h.ctx().LastRect(h.ctx().GetId("##bond")).value();
    // The stack adds its offset (give or take a pixel: the face is that
    // much narrower).
    if (held == 1) {
      single = r.Height();
    } else {
      EXPECT_NEAR(r.Height(), single + CertificateStackOffset(3), 1.5F);
    }
  }
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("##bond"))));
  EXPECT_TRUE(clicked);
}

TEST(PaperTest, PolicySheetShowsExclusions) {
  UiHarness h({700, 600});
  const std::vector<std::string_view> exclusions = {"War zones", "Contraband"};
  PolicySpec spec;
  spec.insurer = {"Teladi Mutual", "Underwriters since day 0", {}};
  spec.title = "Cargo policy";
  spec.insured = "Courier 'Lunar Wind' and its cargo";
  spec.figures = kFigures;
  int nodes_without = 0;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx, {.width = SizeSpec::Grow()});
    PolicySheet(ctx, "##policy", spec);
    End(ctx);
  };
  h.Frame(ui);
  nodes_without = h.ctx().node_count();
  spec.exclusions = exclusions;
  h.Frame(ui);
  // A box, its heading and a line per exclusion.
  EXPECT_EQ(h.ctx().node_count(), nodes_without + 2 + 2);
}

}  // namespace
}  // namespace gloomy::ui
