#ifndef GLOOMY_TESTS_TEST_SUPPORT_CONTRACT_H_
#define GLOOMY_TESTS_TEST_SUPPORT_CONTRACT_H_

#include <string>

namespace gloomy::testing {

/// A contract in the Markdown subset, with `clauses` numbered clauses.
inline auto ContractText(int clauses) -> std::string {
  std::string s =
      "# Supply Contract 42\n\n"
      "Between **Argon Federation Mining** and [Smelter 2](station:2) in "
      "*Ore Belt*. Signed on day 12.\n\n"
      "## Terms\n\n"
      "- Quantity: **500** units of ore, grade A\n"
      "- Price: `1,250 Cr` per unit, <hl>paid on delivery</hl>\n"
      "  - Partial deliveries allowed\n"
      "- Deadline: day 18\n\n"
      "> Late delivery costs 2% of the contract value per day, at most "
      "<color=#ef5b5b>20%</color>.\n\n"
      "```\nref  AFM-0042-ORE\nhash 7f3a 99c1\n```\n\n"
      "---\n\n"
      "## Clauses\n\n";
  for (int i = 1; i <= clauses; ++i) {
    s += "Clause " + std::to_string(i) +
         ": the supplier delivers ore from its mines in the Ore Belt; "
         "deliveries are recorded at the receiving station and settled "
         "weekly.\n\n";
  }
  return s;
}

}  // namespace gloomy::testing

#endif  // GLOOMY_TESTS_TEST_SUPPORT_CONTRACT_H_
