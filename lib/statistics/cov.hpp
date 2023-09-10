//===--- cov.hpp - Utilities for CoV calculation --------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <cmath>
#include <concepts>
#include <iterator>
#include <numeric>
#include <ranges>

namespace llvm_ml::stat {
template <typename T>
concept stat_range = (std::floating_point<std::ranges::range_value_t<T>> || std::integral<std::ranges::range_value_t<T>>) && requires(T a) {
  {a.begin()};
  {a.end()};
};

constexpr double mean(const stat_range auto &input) {
  const size_t size = std::ranges::size(input);

  if (size == 0)
    return std::numeric_limits<double>::quiet_NaN();

  double sum = std::accumulate(input.begin(), input.end(), 0.0);

  return sum / size;
}

constexpr double standard_deviation(const stat_range auto &input, double mean) {
  const size_t size = std::ranges::size(input);

  if (size == 0)
    return std::numeric_limits<double>::quiet_NaN();

  const auto square_diff = [mean](double sum, auto value) {
    double diff = static_cast<double>(value) - mean;
    return sum + (diff * diff);
  };

  double sum = std::accumulate(input.begin(), input.end(), 0.0, square_diff);

  return std::sqrt(sum / size);
}

constexpr double coefficient_of_variation(const stat_range auto &input) {
  const size_t size = std::ranges::size(input);

  if (size == 0)
    return std::numeric_limits<double>::quiet_NaN();

  double mean = llvm_ml::stat::mean(input);
  double sigma = standard_deviation(input, mean);

  return sigma / mean;
}
}
