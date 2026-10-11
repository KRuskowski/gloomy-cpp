#include "gloomy/plot/series.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace gloomy::plot {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

auto MinOf(const std::vector<double> &v) -> double {
  double out = kInf;
  for (const double x : v) {
    if (std::isfinite(x)) {
      out = std::min(out, x);
    }
  }
  return out;
}

auto MaxOf(const std::vector<double> &v) -> double {
  double out = -kInf;
  for (const double x : v) {
    if (std::isfinite(x)) {
      out = std::max(out, x);
    }
  }
  return out;
}

/// One side of a book as steps from the spread outwards.
auto Steps(const std::vector<double> &prices, const std::vector<double> &sizes,
           bool falling) -> std::vector<DepthChart::Step> {
  const std::size_t n = std::min(prices.size(), sizes.size());
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
    return falling ? prices[a] > prices[b] : prices[a] < prices[b];
  });
  std::vector<DepthChart::Step> out;
  double depth = 0.0;
  for (const std::size_t i : order) {
    if (!std::isfinite(prices[i]) || !(sizes[i] > 0.0)) {
      continue;
    }
    depth += sizes[i];
    out.push_back({prices[i], depth});
  }
  return out;
}

}  // namespace

Candlesticks::Candlesticks(matplot::axes_type *parent, std::vector<double> x,
                           std::vector<double> open, std::vector<double> high,
                           std::vector<double> low, std::vector<double> close)
    : matplot::axes_object(parent) {
  SetData(std::move(x),
          std::move(open),
          std::move(high),
          std::move(low),
          std::move(close));
}

void Candlesticks::SetData(std::vector<double> x, std::vector<double> open,
                           std::vector<double> high, std::vector<double> low,
                           std::vector<double> close) {
  const std::size_t n =
      std::min({x.size(), open.size(), high.size(), low.size(), close.size()});
  x.resize(n);
  open.resize(n);
  high.resize(n);
  low.resize(n);
  close.resize(n);
  x_ = std::move(x);
  open_ = std::move(open);
  high_ = std::move(high);
  low_ = std::move(low);
  close_ = std::move(close);
}

auto Candlesticks::xmin() -> double {
  // Half a body beyond the first and last candle.
  const double v = MinOf(x_);
  return x_.size() > 1 ? v - 0.5 * (x_[1] - x_[0]) : v - 0.5;
}

auto Candlesticks::xmax() -> double {
  const double v = MaxOf(x_);
  const std::size_t n = x_.size();
  return n > 1 ? v + 0.5 * (x_[n - 1] - x_[n - 2]) : v + 0.5;
}

auto Candlesticks::ymin() -> double {
  return MinOf(low_);
}

auto Candlesticks::ymax() -> double {
  return MaxOf(high_);
}

DepthChart::DepthChart(matplot::axes_type *parent,
                       std::vector<double> bid_prices,
                       std::vector<double> bid_sizes,
                       std::vector<double> ask_prices,
                       std::vector<double> ask_sizes)
    : matplot::axes_object(parent) {
  SetData(std::move(bid_prices),
          std::move(bid_sizes),
          std::move(ask_prices),
          std::move(ask_sizes));
}

void DepthChart::SetData(std::vector<double> bid_prices,
                         std::vector<double> bid_sizes,
                         std::vector<double> ask_prices,
                         std::vector<double> ask_sizes) {
  bids_ = Steps(bid_prices, bid_sizes, true);
  asks_ = Steps(ask_prices, ask_sizes, false);
}

auto DepthChart::xmin() -> double {
  if (!bids_.empty()) {
    return bids_.back().price;
  }
  return asks_.empty() ? 0.0 : asks_.front().price;
}

auto DepthChart::xmax() -> double {
  if (!asks_.empty()) {
    return asks_.back().price;
  }
  return bids_.empty() ? 1.0 : bids_.front().price;
}

auto DepthChart::ymax() -> double {
  const double b = bids_.empty() ? 0.0 : bids_.back().depth;
  const double a = asks_.empty() ? 0.0 : asks_.back().depth;
  return std::max({a, b, 1e-9});
}

auto AddCandlesticks(matplot::axes_type &axes, std::vector<double> x,
                     std::vector<double> open, std::vector<double> high,
                     std::vector<double> low, std::vector<double> close)
    -> std::shared_ptr<Candlesticks> {
  auto c = std::make_shared<Candlesticks>(&axes,
                                          std::move(x),
                                          std::move(open),
                                          std::move(high),
                                          std::move(low),
                                          std::move(close));
  const bool hold = axes.hold();
  axes.hold(true);
  axes.emplace_object(c);
  axes.hold(hold);
  return c;
}

auto AddDepthChart(matplot::axes_type &axes, std::vector<double> bid_prices,
                   std::vector<double> bid_sizes,
                   std::vector<double> ask_prices,
                   std::vector<double> ask_sizes)
    -> std::shared_ptr<DepthChart> {
  auto d = std::make_shared<DepthChart>(&axes,
                                        std::move(bid_prices),
                                        std::move(bid_sizes),
                                        std::move(ask_prices),
                                        std::move(ask_sizes));
  const bool hold = axes.hold();
  axes.hold(true);
  axes.emplace_object(d);
  axes.hold(hold);
  return d;
}

}  // namespace gloomy::plot
