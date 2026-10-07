#include <gtest/gtest.h>

#include <numeric>
#include <string>
#include <vector>

#include "gloomy/data/columnar_table.h"
#include "gloomy/data/search.h"

namespace gloomy::data {
namespace {

TEST(SearchTest, ParseTermsFoldsAndKeepsQuotes) {
  const auto terms = ParseTerms("  Ore \"Mine 4\"  SMELTER ");
  ASSERT_EQ(terms.size(), 3U);
  EXPECT_EQ(terms[0], "ore");
  EXPECT_EQ(terms[1], "mine 4");
  EXPECT_EQ(terms[2], "smelter");
  EXPECT_TRUE(ParseTerms("   ").empty());
}

TEST(SearchTest, TermsNarrow) {
  const auto a = ParseTerms("or");
  const auto b = ParseTerms("ore");
  const auto c = ParseTerms("ore mine");
  EXPECT_TRUE(TermsNarrow(a, b));
  EXPECT_TRUE(TermsNarrow(b, c));
  EXPECT_TRUE(TermsNarrow({}, a));
  EXPECT_FALSE(TermsNarrow(b, a));
  EXPECT_FALSE(TermsNarrow(c, b));
}

TEST(SearchTest, FindMatchesIsCaseInsensitiveAndMerged) {
  std::vector<MatchRange> m;
  FindMatches("Ore Mine: ORE ore", ParseTerms("ore"), m);
  ASSERT_EQ(m.size(), 3U);
  EXPECT_EQ(m[1].begin, 10U);
  EXPECT_EQ(m[1].end, 13U);
  m.clear();
  FindMatches("abcdef", ParseTerms("abc cde"), m);
  ASSERT_EQ(m.size(), 1U);
  EXPECT_EQ(m[0].begin, 0U);
  EXPECT_EQ(m[0].end, 5U);
  m.clear();
  // Non-ASCII folding keeps offsets.
  FindMatches("Gr\xC3\x9C\x4E", ParseTerms("\xC3\xBCn"), m);
  ASSERT_EQ(m.size(), 1U);
  EXPECT_EQ(m[0].begin, 2U);
  EXPECT_EQ(m[0].end, 5U);
}

auto MakeTable(int rows) -> ColumnarTable {
  ColumnInfo name{.name = "name", .title = "Name", .type = ColumnType::kText};
  ColumnInfo price{
      .name = "price", .title = "Price", .type = ColumnType::kInt64};
  ColumnInfo hidden{.name = "id", .title = "Id", .type = ColumnType::kInt64};
  hidden.searchable = false;
  ColumnarTable t({name, price, hidden});
  const char *kinds[] = {"Ore Mine", "Smelter", "Farm", "Shipyard"};
  for (int i = 0; i < rows; ++i) {
    const std::string n = std::string(kinds[i % 4]) + " " + std::to_string(i);
    t.AddRow({std::string_view(n), std::int64_t{i * 10}, std::int64_t{777}});
  }
  return t;
}

TEST(SearchTest, CorpusFiltersRowsByAllTerms) {
  const ColumnarTable t = MakeTable(100);
  auto corpus = SearchCorpus::Build(t, {});
  ASSERT_TRUE(corpus);
  EXPECT_EQ(corpus->size(), 100U);
  EXPECT_EQ(corpus->columns(), (std::vector<int>{0, 1}));
  EXPECT_EQ(corpus->Doc(1),
            "smelter 1\x1F"
            "10");
  std::vector<std::uint32_t> docs(100);
  std::iota(docs.begin(), docs.end(), 0U);
  ASSERT_TRUE(corpus->Filter(docs, ParseTerms("ore 1")));
  // "Ore Mine 1x" rows: 12, 16 ... and "ore mine 1" itself is row 0? no:
  // row 0 is "Ore Mine 0". Rows i%4==0 containing "1" in name or price.
  for (const std::uint32_t d : docs) {
    EXPECT_EQ(d % 4, 0U);
  }
  EXPECT_FALSE(docs.empty());
  // Hidden columns are not searched: 777 is nowhere.
  std::vector<std::uint32_t> all(100);
  std::iota(all.begin(), all.end(), 0U);
  corpus->Filter(all, ParseTerms("777"));
  EXPECT_TRUE(all.empty());
}

TEST(SearchTest, SegmentAtNamesTheColumn) {
  const ColumnarTable t = MakeTable(4);
  auto corpus = SearchCorpus::Build(t, {});
  EXPECT_EQ(corpus->SegmentAt(2, 0), 0);
  EXPECT_EQ(corpus->SegmentAt(2, 7), 1);
}

TEST(SearchTest, ParallelBuildMatchesSequential) {
  const ColumnarTable t = MakeTable(120000);
  auto corpus = SearchCorpus::Build(t, {});
  ASSERT_EQ(corpus->size(), 120000U);
  EXPECT_EQ(corpus->Doc(119999),
            "shipyard 119999\x1F"
            "1,199,990");
  std::vector<std::uint32_t> docs(120000);
  std::iota(docs.begin(), docs.end(), 0U);
  corpus->Filter(docs, ParseTerms("farm 11999"));
  // Farm rows are i % 4 == 2; with "11999" in name: 11999? no (11999 % 4
  // == 3); 119990..119998 even ones with %4==2: 119990, 119994, 119998.
  EXPECT_EQ(docs, (std::vector<std::uint32_t>{119990, 119994, 119998}));
}

TEST(SearchTest, CancelledBuildReturnsNull) {
  const ColumnarTable t = MakeTable(1000);
  std::atomic<bool> cancel{true};
  EXPECT_EQ(SearchCorpus::Build(t, {}, &cancel), nullptr);
}

}  // namespace
}  // namespace gloomy::data
