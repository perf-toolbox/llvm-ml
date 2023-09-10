//===--- cov.hpp - Utilities for CoV calculation --------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <ranges>
#include <iterator>
#include <concepts>
#include <cmath>

namespace llvm_ml::stat {
template <typename T>
concept stat_range = (std::floating_point<std::ranges::range_value_t<T>> || std::integral<std::ranges::range_value_t<T>>) && requires(T a) {
  {a.begin()};
  {a.end()};
};

constexpr double mean(const stat_range auto &input) {
  double sum = 0.0;

  for (const auto &value : input)
    sum += value;

  return sum / std::ranges::size(input);
}

constexpr double standard_deviation(const stat_range auto &input, double mean) {
  const auto square_diff = [](auto a, double b) {
    double diff = static_cast<double>(a) - b;
    return diff * diff;
  };

  double sum = 0.0;

  for (const auto &value : input)
    sum += square_diff(value, mean);

  return std::sqrt(sum / std::ranges::size(input));
}

constexpr double coefficient_of_variation(const stat_range auto &input) {
  double mean = llvm_ml::stat::mean(input);
  double sigma = standard_deviation(input, mean);

  return mean / sigma;
}
}
