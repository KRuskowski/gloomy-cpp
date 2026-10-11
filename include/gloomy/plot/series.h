#ifndef GLOOMY_PLOT_SERIES_H_
#define GLOOMY_PLOT_SERIES_H_

#include <memory>
#include <string>
#include <vector>

#include <matplot/core/axes_object.h>
#include <matplot/core/axes_type.h>

namespace gloomy::plot {

/// Plot objects matplot++ does not have, for markets: they live in a
/// matplot++ axes like its own lines and bars, and Plot() draws them
/// (matplot++'s gnuplot path skips them).

/// Open, high, low and close per period (OHLC): a wick from low to high
/// and a body from open to close, rising ones in the theme's success
/// colour, falling ones in its error colour.
class Candlesticks : public matplot::axes_object {
 public:
  Candlesticks(matplot::axes_type *parent, std::vector<double> x,
               std::vector<double> open, std::vector<double> high,
               std::vector<double> low, std::vector<double> close);

  auto xmin() -> double override;
  auto xmax() -> double override;
  auto ymin() -> double override;
  auto ymax() -> double override;

  auto plot_string() -> std::string override { return {}; }

  /// Replaces the data (a live series; bump PlotOptions::revision).
  void SetData(std::vector<double> x, std::vector<double> open,
               std::vector<double> high, std::vector<double> low,
               std::vector<double> close);

  [[nodiscard]] auto x() const -> const std::vector<double> & { return x_; }

  [[nodiscard]] auto open() const -> const std::vector<double> & {
    return open_;
  }

  [[nodiscard]] auto high() const -> const std::vector<double> & {
    return high_;
  }

  [[nodiscard]] auto low() const -> const std::vector<double> & { return low_; }

  [[nodiscard]] auto close() const -> const std::vector<double> & {
    return close_;
  }

  /// Body width as a share of the smallest spacing of x, (0, 1].
  [[nodiscard]] auto body_share() const -> double { return body_share_; }

  void body_share(double share) { body_share_ = share; }

 private:
  std::vector<double> x_;
  std::vector<double> open_;
  std::vector<double> high_;
  std::vector<double> low_;
  std::vector<double> close_;
  double body_share_ = 0.7;
};

/// An order book's depth: bids (buy orders) and asks (sell orders) by
/// price, each side's cumulative size stepping away from the spread, the
/// bids in the success colour on the left, the asks in the error colour
/// on the right.
class DepthChart : public matplot::axes_object {
 public:
  /// Prices and sizes of each side, in any order.
  DepthChart(matplot::axes_type *parent, std::vector<double> bid_prices,
             std::vector<double> bid_sizes, std::vector<double> ask_prices,
             std::vector<double> ask_sizes);

  auto xmin() -> double override;
  auto xmax() -> double override;

  auto ymin() -> double override { return 0.0; }

  auto ymax() -> double override;

  auto plot_string() -> std::string override { return {}; }

  void SetData(std::vector<double> bid_prices, std::vector<double> bid_sizes,
               std::vector<double> ask_prices, std::vector<double> ask_sizes);

  /// The steps of a side: price and cumulative size, from the spread
  /// outwards (bids by falling price, asks by rising price).
  struct Step {
    double price = 0.0;
    double depth = 0.0;
  };

  [[nodiscard]] auto bids() const -> const std::vector<Step> & { return bids_; }

  [[nodiscard]] auto asks() const -> const std::vector<Step> & { return asks_; }

 private:
  std::vector<Step> bids_;
  std::vector<Step> asks_;
};

/// Adds candlesticks to `axes` (keeping what is there).
auto AddCandlesticks(matplot::axes_type &axes, std::vector<double> x,
                     std::vector<double> open, std::vector<double> high,
                     std::vector<double> low, std::vector<double> close)
    -> std::shared_ptr<Candlesticks>;

/// Adds a depth chart to `axes` (keeping what is there).
auto AddDepthChart(matplot::axes_type &axes, std::vector<double> bid_prices,
                   std::vector<double> bid_sizes,
                   std::vector<double> ask_prices,
                   std::vector<double> ask_sizes)
    -> std::shared_ptr<DepthChart>;

}  // namespace gloomy::plot

#endif  // GLOOMY_PLOT_SERIES_H_
