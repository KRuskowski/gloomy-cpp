#include "gloomy/ui/paper.h"

#include <algorithm>
#include <string>

#include "gloomy/ui/hud.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

// Pixels between stacked certificates, and how many show.
constexpr float kStackStep = 5.0F;
constexpr int kMaxStacked = 4;
// Width of the ornate band of a certificate.
constexpr float kOrnateBand = 16.0F;

/// The skin to draw with: the given one, else the theme's (on paper).
auto SkinOf(const Context &ctx, const PanelSkin *skin, bool on_paper)
    -> PanelSkin {
  if (skin != nullptr) {
    return *skin;
  }
  const PanelSkin s = PanelSkin::FromTheme(ctx.theme());
  return on_paper ? s.OnPaper() : s;
}

/// A text leaf.
void Txt(Context &ctx, std::string_view text, text::FontId font, float size,
         Color color, bool wrap = false, SizeSpec width = SizeSpec::Fit(),
         TextAlign align = TextAlign::kLeft) {
  TextSpec spec;
  spec.text = text;
  spec.font = font;
  spec.size = size;
  spec.color = color;
  spec.wrap = wrap;
  spec.align = align;
  LayoutSpec layout;
  layout.width = width;
  ctx.Text(spec, layout);
}

/// Capitals of ASCII letters (small-caps labels).
auto Caps(std::string_view s) -> std::string {
  std::string out(s);
  for (char &c : out) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
  }
  return out;
}

auto HouseColor(const Letterhead &head, const PanelSkin &skin) -> Color {
  return head.color.a > 0.0F ? head.color : skin.ink;
}

/// The issuer's strip across the top: icon, name, line, in the house
/// colour.
void LetterheadStrip(Context &ctx, const Letterhead &head,
                     const PanelSkin &skin) {
  const Theme &t = ctx.theme();
  const Color house = HouseColor(head, skin);
  NodeDesc strip;
  strip.layout.mode = LayoutMode::kRow;
  strip.layout.width = SizeSpec::Grow();
  strip.layout.padding = {10, 6, 10, 6};
  strip.layout.gap = 10;
  strip.layout.align = Align::kCenter;
  strip.style.fill = Mix(skin.paper_edge, house, 0.22F);
  strip.style.radii = gfx::CornerRadii::All(2.0F);
  const int node = ctx.Open(strip);
  ctx.AddPainter(node, [house](PaintContext &pc, const Rect &r) {
    pc.list.FillRect({r.x0, r.y0, r.x0 + 4, r.y1}, house);
    pc.list.FillRect({r.x0, r.y1 - 2, r.x1, r.y1}, house.WithAlpha(0.7F));
  });
  if (head.icon != icon::kNoIcon) {
    Icon(ctx, head.icon, 22.0F, house);
  }
  LayoutSpec lines;
  lines.gap = 1;
  lines.width = SizeSpec::Grow();
  BeginColumn(ctx, lines);
  Txt(ctx, head.name, t.fonts.bold, t.metrics.font_size + 1, skin.text);
  if (!head.line.empty()) {
    Txt(ctx,
        head.line,
        t.fonts.sans,
        t.metrics.small_font_size,
        skin.text_muted);
  }
  End(ctx);
  ctx.Close();
}

/// A box reserving room for a stamp, painted in it.
void StampBox(Context &ctx, const Stamp &stamp, const PanelSkin &skin,
              float size) {
  if (stamp.text.empty()) {
    return;
  }
  const Theme &t = ctx.theme();
  const Vec2 sz = StampSize(ctx.text(), t.fonts.bold, size, stamp.text);
  NodeDesc box;
  box.layout.width = SizeSpec::Fixed(sz.x + 4);
  box.layout.height = SizeSpec::Fixed(sz.y + 4);
  const int node = ctx.Open(box);
  ctx.Close();
  const Color color = skin.ToneInk(stamp.tone);
  const std::string text(stamp.text);
  const text::FontId bold = t.fonts.bold;
  ctx.AddPainter(
      node, [color, text, bold, size](PaintContext &pc, const Rect &r) {
        PaintStamp(pc.list, pc.text, bold, size, r.Center(), text, color);
      });
}

/// Kind, title and reference with the stamp at the right.
void TitleBlock(Context &ctx, std::string_view kind, std::string_view title,
                std::string_view reference, const Stamp &stamp,
                const PanelSkin &skin) {
  const Theme &t = ctx.theme();
  LayoutSpec row;
  row.width = SizeSpec::Grow();
  row.gap = 12;
  row.align = Align::kCenter;
  BeginRow(ctx, row);
  LayoutSpec col;
  col.gap = 2;
  col.width = SizeSpec::Grow();
  BeginColumn(ctx, col);
  if (!kind.empty()) {
    Txt(ctx, Caps(kind), t.fonts.bold, t.metrics.small_font_size, skin.ink);
  }
  Txt(ctx,
      title,
      t.fonts.bold,
      t.metrics.heading_font_size,
      skin.text,
      true,
      SizeSpec::Grow());
  if (!reference.empty()) {
    Txt(ctx,
        reference,
        t.fonts.sans,
        t.metrics.small_font_size,
        skin.text_muted);
  }
  End(ctx);
  StampBox(ctx, stamp, skin, t.metrics.heading_font_size);
  End(ctx);
}

/// The figures as boxes: label in small capitals, value big, a bar in
/// the tone's colour.
auto Figures(Context &ctx, std::span<const SheetFigure> figures,
             const PanelSkin &skin, float value_size) -> int {
  if (figures.empty()) {
    return -1;
  }
  const Theme &t = ctx.theme();
  int clicked = -1;
  LayoutSpec row;
  row.width = SizeSpec::Grow();
  row.gap = 8;
  BeginRow(ctx, row);
  ctx.PushId("##figures");
  for (std::size_t i = 0; i < figures.size(); ++i) {
    const SheetFigure &f = figures[i];
    const Color tone =
        f.tone == Tone::kNeutral ? skin.ink_muted : skin.ToneInk(f.tone);
    NodeDesc box;
    box.id = ctx.GetId(static_cast<std::uint64_t>(i) + 1);
    box.layout.mode = LayoutMode::kColumn;
    box.layout.width = SizeSpec::Grow();
    box.layout.padding = {10, 5, 8, 6};
    box.layout.gap = 1;
    box.style.fill = skin.paper_edge;
    box.style.radii = gfx::CornerRadii::All(2.0F);
    box.flags = kNodeInteractive;
    const int node = ctx.Open(box);
    ctx.AddPainter(node, [tone](PaintContext &pc, const Rect &r) {
      pc.list.FillRect({r.x0, r.y0, r.x0 + 3, r.y1}, tone);
    });
    Txt(ctx,
        Caps(f.label),
        t.fonts.bold,
        t.metrics.small_font_size - 1,
        skin.text_muted);
    Txt(ctx,
        f.value,
        t.fonts.bold,
        value_size,
        f.tone == Tone::kNeutral ? skin.text : skin.ToneInk(f.tone),
        true,
        SizeSpec::Grow());
    ctx.Close();
    if (ctx.Interact(box.id).clicked) {
      clicked = static_cast<int>(i);
    }
    if (!f.why.empty()) {
      Tooltip(ctx, box.id, f.why);
    }
  }
  ctx.PopId();
  End(ctx);
  return clicked;
}

/// Signature lines: the name over a rule (in the party's colour once
/// signed), the role and date under it.
void Signatures(Context &ctx, std::span<const Signatory> parties,
                const PanelSkin &skin) {
  if (parties.empty()) {
    return;
  }
  const Theme &t = ctx.theme();
  LayoutSpec row;
  row.width = SizeSpec::Grow();
  row.gap = 18;
  row.padding = {0, 4, 0, 0};
  BeginRow(ctx, row);
  for (const Signatory &p : parties) {
    LayoutSpec col;
    col.width = SizeSpec::Grow();
    col.gap = 2;
    const int node = BeginColumn(ctx, col);
    const Color ink = p.color.a > 0.0F ? p.color : skin.text;
    if (p.signed_) {
      Txt(ctx, p.name, t.fonts.italic, t.metrics.font_size + 2, ink);
    } else {
      Txt(ctx,
          "awaiting signature",
          t.fonts.italic,
          t.metrics.font_size,
          skin.text_muted);
    }
    NodeDesc rule;
    rule.layout.width = SizeSpec::Grow();
    rule.layout.height = SizeSpec::Fixed(1);
    rule.style.fill = skin.ink_muted.WithAlpha(0.7F);
    ctx.Open(rule);
    ctx.Close();
    std::string under = Caps(p.role);
    if (!p.signed_ && !p.name.empty()) {
      under += "  " + std::string(p.name);
    }
    if (!p.date.empty()) {
      under += "  " + std::string(p.date);
    }
    Txt(ctx,
        under,
        t.fonts.sans,
        t.metrics.small_font_size - 1,
        skin.text_muted);
    End(ctx);
    if (p.signed_) {
      const Color seal = ink.WithAlpha(0.55F);
      ctx.AddPainter(node, [seal](PaintContext &pc, const Rect &r) {
        PaintSeal(pc.list, {r.x1 - 12, r.y0 + 10}, 9.0F, seal, 16);
      });
    }
  }
  End(ctx);
}

/// The paper's frame and scan lines, painted under the content.
void PaperPainter(Context &ctx, int node, const PanelSkin &skin, Color edge) {
  const PanelSkin s = skin;
  ctx.AddPainter(
      node,
      [s, edge](PaintContext &pc, const Rect &r) {
        pc.list.Shadow(r,
                       Color{0, 0, 0, 0.45F},
                       10.0F,
                       {0, 3},
                       gfx::CornerRadii::All(s.corner));
        PaintFrame(pc.list, r, s, s.paper, edge);
        if (s.scanlines > 0.0F) {
          Scanlines(
              pc.list, r.Inset(s.frame), 3.0F, s.ink.WithAlpha(s.scanlines));
        }
      },
      true);
}

/// "Clauses (N)" toggle and, open, the numbered clauses.
auto Clauses(Context &ctx, std::span<const std::string_view> clauses,
             const PanelSkin &skin, Id id) -> SheetResult {
  SheetResult result;
  if (clauses.empty()) {
    return result;
  }
  const Theme &t = ctx.theme();
  bool &open = ctx.State<bool>(id);
  NodeDesc rule;
  rule.layout.width = SizeSpec::Grow();
  rule.layout.height = SizeSpec::Fixed(1);
  rule.style.fill = skin.ink_muted.WithAlpha(0.4F);
  ctx.Open(rule);
  ctx.Close();
  NodeDesc toggle;
  toggle.id = HashCombine(id, 0x636CULL);
  toggle.layout.mode = LayoutMode::kRow;
  toggle.layout.gap = 6;
  toggle.layout.padding = Padding::Xy(2, 2);
  toggle.layout.align = Align::kCenter;
  toggle.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const Interaction it = ctx.Interact(toggle.id);
  if (it.clicked || (ctx.HasFocus(toggle.id) && ctx.ConsumeKey(Key::kEnter))) {
    open = !open;
    result.clauses_toggled = true;
  }
  ctx.Open(toggle);
  const std::string text = std::string(open ? "- " : "+ ") + "Clauses (" +
                           std::to_string(clauses.size()) + ")";
  Txt(ctx,
      text,
      t.fonts.bold,
      t.metrics.small_font_size,
      it.hovered ? skin.text : skin.ink);
  if (!open) {
    Txt(ctx,
        "the fine print",
        t.fonts.italic,
        t.metrics.small_font_size,
        skin.text_muted);
  }
  ctx.Close();
  if (open) {
    LayoutSpec list;
    list.gap = 3;
    list.width = SizeSpec::Grow();
    list.padding = {6, 0, 0, 0};
    BeginColumn(ctx, list);
    for (std::size_t i = 0; i < clauses.size(); ++i) {
      LayoutSpec line;
      line.gap = 6;
      line.width = SizeSpec::Grow();
      BeginRow(ctx, line);
      Txt(ctx,
          "§" + std::to_string(i + 1),
          t.fonts.bold,
          t.metrics.small_font_size,
          skin.ink);
      Txt(ctx,
          clauses[i],
          t.fonts.sans,
          t.metrics.small_font_size,
          skin.text,
          true,
          SizeSpec::Grow());
      End(ctx);
    }
    End(ctx);
  }
  result.clauses_open = open;
  return result;
}

struct SheetFrame {
  Id id = kNoId;
  int figure = -1;
};

/// The sheets opened and not yet closed, innermost last, for
/// EndContractSheet().
auto OpenSheets() -> std::vector<SheetFrame> & {
  static thread_local std::vector<SheetFrame> sheets;
  return sheets;
}

}  // namespace

auto BeginContractSheet(Context &ctx, std::string_view label,
                        const ContractSheetSpec &spec) -> int {
  const PanelSkin skin = SkinOf(ctx, spec.skin, false);
  NodeDesc sheet;
  sheet.id = ctx.GetId(label);
  sheet.layout.mode = LayoutMode::kColumn;
  sheet.layout.width = SizeSpec::Grow();
  sheet.layout.padding = Padding::All(skin.frame + 9);
  sheet.layout.gap = 8;
  sheet.layout.align = Align::kStretch;
  const int node = ctx.Open(sheet);
  PaperPainter(ctx,
               node,
               skin,
               Mix(skin.ink_muted, HouseColor(spec.issuer, skin), 0.5F));
  ctx.PushId(sheet.id);
  LetterheadStrip(ctx, spec.issuer, skin);
  TitleBlock(ctx, spec.kind, spec.title, spec.reference, spec.stamp, skin);
  const int figure = Figures(
      ctx, spec.figures, skin, ctx.theme().metrics.heading_font_size + 4);
  OpenSheets().push_back({sheet.id, figure});
  return node;
}

auto EndContractSheet(Context &ctx, const ContractSheetSpec &spec)
    -> SheetResult {
  const PanelSkin skin = SkinOf(ctx, spec.skin, false);
  SheetFrame frame;
  if (!OpenSheets().empty()) {
    frame = OpenSheets().back();
    OpenSheets().pop_back();
  }
  Signatures(ctx, spec.parties, skin);
  SheetResult result =
      Clauses(ctx, spec.clauses, skin, HashCombine(frame.id, 0x434CULL));
  result.figure = frame.figure;
  ctx.PopId();
  ctx.Close();
  return result;
}

auto CertificateStackOffset(int held) -> float {
  const int behind = std::clamp(held - 1, 0, kMaxStacked - 1);
  return static_cast<float>(behind) * kStackStep;
}

auto Certificate(Context &ctx, std::string_view label,
                 const CertificateSpec &spec) -> bool {
  const Theme &t = ctx.theme();
  const PanelSkin skin = SkinOf(ctx, spec.skin, true);
  const Color house = Mix(HouseColor(spec.issuer, skin), skin.ink, 0.25F);
  const float stack = CertificateStackOffset(spec.held);
  const int behind = static_cast<int>(stack / kStackStep);
  // The stack: an outer box with room for the sheets behind.
  NodeDesc outer;
  outer.id = ctx.GetId(label);
  outer.layout.mode = LayoutMode::kColumn;
  outer.layout.width = SizeSpec::Grow();
  outer.layout.padding = {0, 0, stack, stack};
  outer.layout.align = Align::kStretch;
  outer.flags = kNodeInteractive;
  const int outer_node = ctx.Open(outer);
  const PanelSkin s = skin;
  ctx.AddPainter(
      outer_node,
      [s, behind, house](PaintContext &pc, const Rect &r) {
        const float st = static_cast<float>(behind) * kStackStep;
        for (int i = behind; i >= 1; --i) {
          const float o = static_cast<float>(i) * kStackStep;
          const Rect sheet{r.x0 + o, r.y0 + o, r.x1 - st + o, r.y1 - st + o};
          pc.list.Shadow(sheet, Color{0, 0, 0, 0.3F}, 4.0F, {0, 1});
          pc.list.FillRect(sheet, Mix(s.paper, s.paper_edge, 0.6F));
          pc.list.StrokeRect(sheet, house.WithAlpha(0.6F), 1.0F);
        }
      },
      true);
  NodeDesc face;
  face.layout.mode = LayoutMode::kColumn;
  face.layout.width = SizeSpec::Grow();
  face.layout.padding = Padding::All(kOrnateBand + 10);
  face.layout.gap = 5;
  face.layout.align = Align::kCenter;
  const int face_node = ctx.Open(face);
  const Ornament ornament = OrnamentOf(spec.ornament_seed);
  ctx.AddPainter(
      face_node,
      [s, house, ornament](PaintContext &pc, const Rect &r) {
        pc.list.Shadow(r, Color{0, 0, 0, 0.4F}, 8.0F, {0, 2});
        pc.list.FillRect(r, s.paper);
        // A faint guilloche wash behind the text.
        const Rect in = r.Inset(kOrnateBand + 2);
        for (int i = 1; i <= 3; ++i) {
          StrokeEllipse(pc.list,
                        in.Inset(static_cast<float>(i) * 9.0F),
                        1.0F,
                        house.WithAlpha(0.08F),
                        64);
        }
        PaintOrnateBorder(
            pc.list, r.Inset(3), ornament, kOrnateBand, house.WithAlpha(0.85F));
      },
      true);
  ctx.PushId(outer.id);
  // Issuer and title.
  LayoutSpec head;
  head.gap = 8;
  head.align = Align::kCenter;
  BeginRow(ctx, head);
  if (spec.issuer.icon != icon::kNoIcon) {
    Icon(ctx, spec.issuer.icon, 18.0F, house);
  }
  Txt(ctx,
      Caps(spec.issuer.name),
      t.fonts.bold,
      t.metrics.small_font_size,
      house);
  End(ctx);
  if (!spec.issuer.line.empty()) {
    Txt(ctx,
        spec.issuer.line,
        t.fonts.italic,
        t.metrics.small_font_size,
        skin.text_muted);
  }
  Txt(ctx,
      Caps(spec.title),
      t.fonts.bold,
      t.metrics.heading_font_size + 2,
      skin.text);
  // The face value, big.
  Txt(ctx, spec.face, t.fonts.bold, t.metrics.heading_font_size * 2.0F, house);
  if (!spec.face_label.empty()) {
    Txt(ctx,
        Caps(spec.face_label),
        t.fonts.sans,
        t.metrics.small_font_size - 1,
        skin.text_muted);
  }
  // Terms in a row.
  if (!spec.terms.empty()) {
    LayoutSpec terms;
    terms.gap = 18;
    terms.padding = {0, 4, 0, 2};
    BeginRow(ctx, terms);
    for (std::size_t i = 0; i < spec.terms.size(); ++i) {
      const SheetFigure &f = spec.terms[i];
      NodeDesc cell;
      cell.id = ctx.GetId(static_cast<std::uint64_t>(i) + 1);
      cell.layout.mode = LayoutMode::kColumn;
      cell.layout.align = Align::kCenter;
      cell.layout.gap = 1;
      cell.flags = kNodeInteractive;
      ctx.Open(cell);
      Txt(ctx,
          Caps(f.label),
          t.fonts.sans,
          t.metrics.small_font_size - 1,
          skin.text_muted);
      Txt(ctx,
          f.value,
          t.fonts.bold,
          t.metrics.font_size + 1,
          f.tone == Tone::kNeutral ? skin.text : skin.ToneInk(f.tone));
      ctx.Close();
      if (!f.why.empty()) {
        Tooltip(ctx, cell.id, f.why);
      }
    }
    End(ctx);
  }
  if (!spec.risk.empty()) {
    const Color risk = skin.ToneInk(spec.risk_tone);
    NodeDesc box;
    box.layout.mode = LayoutMode::kRow;
    box.layout.padding = Padding::Xy(10, 3);
    box.style.border = risk.WithAlpha(0.7F);
    box.style.border_width = 1.0F;
    box.style.radii = gfx::CornerRadii::All(2.0F);
    ctx.Open(box);
    Txt(ctx, spec.risk, t.fonts.bold, t.metrics.small_font_size, risk);
    ctx.Close();
  }
  // Serial at the left, holding at the right, seal and stamp painted.
  LayoutSpec foot;
  foot.width = SizeSpec::Grow();
  foot.gap = 8;
  foot.align = Align::kCenter;
  foot.padding = {0, 4, 0, 0};
  const int foot_node = BeginRow(ctx, foot);
  Txt(ctx,
      spec.serial,
      t.fonts.sans,
      t.metrics.small_font_size - 1,
      skin.text_muted);
  Spacer(ctx);
  StampBox(ctx, spec.stamp, skin, t.metrics.font_size);
  if (spec.held > 1) {
    Txt(ctx,
        "x" + std::to_string(spec.held) + " held",
        t.fonts.bold,
        t.metrics.small_font_size,
        skin.text);
  }
  End(ctx);
  ctx.PopId();
  ctx.Close();
  // The issuer's seal, unless a stamp takes its place.
  if (spec.stamp.text.empty()) {
    const Color seal = house.WithAlpha(0.7F);
    ctx.AddPainter(foot_node, [seal](PaintContext &pc, const Rect &r) {
      PaintSeal(pc.list, {r.Center().x, r.Center().y - 2}, 13.0F, seal);
    });
  }
  ctx.Close();
  return ctx.Interact(outer.id).clicked;
}

auto PolicySheet(Context &ctx, std::string_view label, const PolicySpec &spec)
    -> bool {
  const Theme &t = ctx.theme();
  const PanelSkin skin = SkinOf(ctx, spec.skin, true);
  NodeDesc sheet;
  sheet.id = ctx.GetId(label);
  sheet.layout.mode = LayoutMode::kColumn;
  sheet.layout.width = SizeSpec::Grow();
  sheet.layout.padding = Padding::All(skin.frame + 9);
  sheet.layout.gap = 8;
  sheet.layout.align = Align::kStretch;
  sheet.flags = kNodeInteractive;
  const int node = ctx.Open(sheet);
  PaperPainter(ctx, node, skin, HouseColor(spec.insurer, skin));
  ctx.PushId(sheet.id);
  LetterheadStrip(ctx, spec.insurer, skin);
  TitleBlock(
      ctx, "Insurance policy", spec.title, spec.number, spec.stamp, skin);
  // What is insured, as a boxed line.
  if (!spec.insured.empty()) {
    NodeDesc box;
    box.layout.mode = LayoutMode::kColumn;
    box.layout.width = SizeSpec::Grow();
    box.layout.padding = Padding::Xy(10, 6);
    box.layout.gap = 1;
    box.style.border = skin.ink_muted.WithAlpha(0.6F);
    box.style.border_width = 1.0F;
    box.style.radii = gfx::CornerRadii::All(2.0F);
    ctx.Open(box);
    Txt(ctx,
        "INSURED",
        t.fonts.bold,
        t.metrics.small_font_size - 1,
        skin.text_muted);
    Txt(ctx,
        spec.insured,
        t.fonts.bold,
        t.metrics.font_size + 1,
        skin.text,
        true,
        SizeSpec::Grow());
    ctx.Close();
  }
  Figures(ctx, spec.figures, skin, t.metrics.font_size + 3);
  // Exclusions: hatched, in the bad tone, said plainly.
  if (!spec.exclusions.empty()) {
    const Color bad = skin.bad;
    NodeDesc box;
    box.layout.mode = LayoutMode::kColumn;
    box.layout.width = SizeSpec::Grow();
    box.layout.padding = {12, 6, 10, 7};
    box.layout.gap = 2;
    box.style.border = bad.WithAlpha(0.8F);
    box.style.border_width = 1.5F;
    box.style.radii = gfx::CornerRadii::All(2.0F);
    const int ex = ctx.Open(box);
    ctx.AddPainter(
        ex,
        [bad](PaintContext &pc, const Rect &r) {
          PaintHatch(pc.list, r.Inset(2), bad.WithAlpha(0.08F), 7.0F, 2.0F);
          pc.list.FillRect({r.x0, r.y0, r.x0 + 4, r.y1}, bad);
        },
        true);
    Txt(ctx, "NOT COVERED", t.fonts.bold, t.metrics.small_font_size, bad);
    for (const std::string_view e : spec.exclusions) {
      Txt(ctx,
          "- " + std::string(e),
          t.fonts.sans,
          t.metrics.small_font_size,
          skin.text,
          true,
          SizeSpec::Grow());
    }
    ctx.Close();
  }
  Signatures(ctx, spec.parties, skin);
  ctx.PopId();
  ctx.Close();
  return ctx.Interact(sheet.id).clicked;
}

}  // namespace gloomy::ui
