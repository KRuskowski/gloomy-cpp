#include "sample_data.h"

#include <cmath>
#include <vector>

namespace gloomy::apps {

namespace {

constexpr const char *kSectors[] = {"Argon Prime",
                                    "Home of Light",
                                    "Red Light",
                                    "Ore Belt",
                                    "Cloudbase NW",
                                    "Three Worlds",
                                    "Power Circle",
                                    "Herron's Nebula",
                                    "The Wall",
                                    "Antigone Memorial"};
constexpr const char *kGoods[] = {"Energy Cells",
                                  "Ore",
                                  "Silicon Wafers",
                                  "Food Rations",
                                  "Ship Parts",
                                  "Crystals",
                                  "Water",
                                  "Medical Supplies"};
constexpr double kBase[] = {16, 50, 250, 30, 900, 1700, 12, 120};
constexpr const char *kOwners[] = {"Argon Federation",
                                   "Teladi Company",
                                   "Boron Kingdom",
                                   "Paranid Empire",
                                   "Player"};

auto Col(const char *name, const char *title, data::ColumnType type)
    -> data::ColumnInfo {
  data::ColumnInfo c;
  c.name = name;
  c.title = title;
  c.type = type;
  return c;
}

auto MarketColumns() -> std::vector<data::ColumnInfo> {
  using data::ColumnType;
  std::vector<data::ColumnInfo> c(8);
  c[kStation] = Col("station", "Station", ColumnType::kText);
  c[kSector] = Col("sector", "Sector", ColumnType::kText);
  c[kGood] = Col("good", "Good", ColumnType::kText);
  c[kKind] = Col("kind", "Offer", ColumnType::kEnum);
  c[kKind].enum_labels = {"Sells", "Buys", "Both"};
  c[kPrice] = Col("price", "Price", ColumnType::kDouble);
  c[kPrice].unit = " Cr";
  c[kPrice].explained = true;
  c[kStock] = Col("stock", "Stock", ColumnType::kInt64);
  c[kAge] = Col("age", "Report age", ColumnType::kDouble);
  c[kAge].decimals = 0;
  c[kAge].unit = " min";
  c[kOwner] = Col("owner", "Owner", ColumnType::kText);
  return c;
}

}  // namespace

SampleMarket::SampleMarket(int rows) : data::ColumnarTable(MarketColumns()) {
  Reserve(static_cast<data::RowIndex>(rows));
  std::uint64_t x = 0x9E3779B97F4A7C15ULL;
  auto next = [&](std::uint32_t mod) {
    x ^= x << 13U;
    x ^= x >> 7U;
    x ^= x << 17U;
    return static_cast<std::uint32_t>(x % mod);
  };
  std::string station;
  for (int i = 0; i < rows; ++i) {
    const int good = static_cast<int>(next(8));
    const int sector = static_cast<int>(next(10));
    // "Herron's Nebula" -> "Herron's 12-3"; "The Wall" -> "Wall 12-3".
    std::string_view words(kSectors[sector]);
    if (words.starts_with("The ")) {
      words.remove_prefix(4);
    }
    station = std::string(words.substr(0, words.find(' '))) + " " +
              std::to_string(i / 4) + "-" + std::to_string(i % 4);
    const double premium = (static_cast<double>(next(1000)) - 450.0) / 1000.0;
    const double price = kBase[good] * (1.0 + premium);
    AddRow({std::string_view(station),
            std::string_view(kSectors[sector]),
            std::string_view(kGoods[good]),
            std::int64_t{static_cast<std::int64_t>(next(3))},
            std::round(price * 100.0) / 100.0,
            std::int64_t{static_cast<std::int64_t>(next(50000))},
            static_cast<double>(next(240)),
            std::string_view(kOwners[next(5)])});
  }
}

auto SampleMarket::Explain(data::RowIndex row, int col,
                           data::Explanation &out) const -> bool {
  if (col != kPrice) {
    return false;
  }
  const std::string_view good = GetText(kGood, row);
  double base = 0.0;
  for (std::size_t g = 0; g < 8; ++g) {
    if (good == kGoods[g]) {
      base = kBase[g];
    }
  }
  const double price = GetDouble(kPrice, row);
  const double pct = base > 0 ? (price / base - 1.0) * 100.0 : 0.0;
  const std::int64_t stock = GetInt64(kStock, row);
  if (pct > 15) {
    out.reason = "Shortage: stock covers less than a day of demand";
  } else if (pct < -15) {
    out.reason = "Surplus: " + std::to_string(stock) + " units waiting";
  } else {
    out.reason = "Near the average for " + std::string(good);
  }
  out.breakdown = {{"Base price", base}, {"Market premium", price - base}};
  return true;
}

auto SampleMarket::AveragePrice(std::string_view good) const -> double {
  double sum = 0.0;
  std::int64_t n = 0;
  for (data::RowIndex r = 0; r < RowCount(); ++r) {
    if (GetText(kGood, r) == good) {
      sum += GetDouble(kPrice, r);
      ++n;
    }
  }
  return n > 0 ? sum / static_cast<double>(n) : 0.0;
}

auto SampleContract(int clauses) -> std::string {
  std::string s =
      "# Supply Contract AFM-0042\n\n"
      "Between **Argon Federation Mining** (the *supplier*) and "
      "[Smelter 2](station:smelter-2) in the Ore Belt (the *buyer*). "
      "Signed on day 12 at [Argon Prime Trading Station](station:apts).\n\n"
      "## Terms\n\n"
      "- Quantity: **500** units of ore, grade A\n"
      "- Price: `1,250 Cr` per unit, <hl>paid on delivery</hl>\n"
      "  - Partial deliveries are allowed\n"
      "  - Each delivery is invoiced separately\n"
      "- Deadline: day 18, 12:00 station time\n\n"
      "> Late delivery costs 2% of the contract value per day, at most "
      "<color=#ef5b5b>20%</color> of the value.\n\n"
      "## Delivery\n\n"
      "Ore is delivered to the receiving dock of Smelter 2. The buyer "
      "confirms each delivery within one hour; unconfirmed deliveries count "
      "as accepted.\n\n"
      "```\nreference  AFM-0042-ORE\nescrow     12,500 Cr (10%)\n```\n\n"
      "---\n\n"
      "## Clauses\n\n";
  static const char *kTopics[] = {
      "Quality is checked by sampling one container in ten.",
      "Ore below grade A may be refused or priced down by 30%.",
      "Pirate losses en route are the supplier's risk.",
      "Either side may end the contract with three days' notice.",
      "Disputes go to the Argon Federation Trade Court.",
      "Escrow is released after the final delivery is confirmed."};
  for (int i = 1; i <= clauses; ++i) {
    s += "**Clause " + std::to_string(i) + ".** " + kTopics[i % 6] +
         " Deliveries are recorded at the receiving station and settled "
         "weekly.\n\n";
  }
  return s;
}

}  // namespace gloomy::apps
