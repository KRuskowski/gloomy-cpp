#include "gloomy/data/document.h"

#include <gtest/gtest.h>

namespace gloomy::data {
namespace {

TEST(DocumentTest, InlineMarkup) {
  DocBlock b;
  Document::ParseInline(
      "Pay **500** Cr to [Mine 4](station:4) by *day 3*, `ref 7*2` "
      "<hl>now</hl> <color=#ff0000>late</color> \\*x",
      b);
  EXPECT_EQ(b.text, "Pay 500 Cr to Mine 4 by day 3, ref 7*2 now late *x");
  ASSERT_EQ(b.spans.size(), 6U);
  EXPECT_EQ(b.spans[0].style, kSpanBold);
  EXPECT_EQ(b.text.substr(b.spans[0].begin, b.spans[0].end - b.spans[0].begin),
            "500");
  EXPECT_EQ(b.spans[1].style, kSpanLink);
  EXPECT_EQ(b.spans[1].link, "station:4");
  EXPECT_EQ(b.spans[2].style, kSpanItalic);
  EXPECT_EQ(b.spans[3].style, kSpanMono);
  EXPECT_EQ(b.spans[4].style, kSpanHighlight);
  EXPECT_EQ(b.spans[5].color, 0xFF0000FFU);
}

TEST(DocumentTest, BlockStructure) {
  const Document doc = Document::ParseMarkdown(R"(# Contract 42

Deliver ore
to Smelter 2.

## Terms

- Quantity: 500
  - Grade A only
1. First payment
> Penalty clauses apply
> after day 3.

```
code line 1
code line 2
```
---
Last.)");
  const auto blocks = doc.blocks();
  ASSERT_EQ(blocks.size(), 10U);
  EXPECT_EQ(blocks[0].kind, BlockKind::kHeading);
  EXPECT_EQ(blocks[0].level, 1);
  EXPECT_EQ(blocks[1].text, "Deliver ore to Smelter 2.");
  EXPECT_EQ(blocks[2].level, 2);
  EXPECT_EQ(blocks[3].kind, BlockKind::kListItem);
  EXPECT_EQ(blocks[4].level, 1);
  EXPECT_EQ(blocks[5].text, "1. First payment");
  EXPECT_EQ(blocks[6].kind, BlockKind::kQuote);
  EXPECT_EQ(blocks[6].text, "Penalty clauses apply after day 3.");
  EXPECT_EQ(blocks[7].kind, BlockKind::kCode);
  EXPECT_EQ(blocks[7].text, "code line 1\ncode line 2");
  EXPECT_EQ(blocks[8].kind, BlockKind::kRule);
  EXPECT_EQ(blocks[9].text, "Last.");
  EXPECT_EQ(doc.Outline().size(), 2U);
  EXPECT_EQ(doc.Outline()[1].title, "Terms");
}

TEST(DocumentTest, FindAllAcrossBlocks) {
  const Document doc = Document::ParseMarkdown(
      "Ore and more ore.\n\nNo match here.\n\nORE again.");
  const auto matches = doc.FindAll(ParseTerms("ore"));
  ASSERT_EQ(matches.size(), 4U);
  EXPECT_EQ(matches[0].block, 0U);
  EXPECT_EQ(matches[1].range.begin, 9U);
  EXPECT_EQ(matches[3].block, 2U);
  EXPECT_TRUE(doc.FindAll({}).empty());
}

}  // namespace
}  // namespace gloomy::data
