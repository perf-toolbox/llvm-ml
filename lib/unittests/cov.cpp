//===--- cov.hpp - CoV calculation tests ----------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/statistics/cov.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <iostream>

TEST_CASE("Mean is computed correctly", "[statistics/cov.hpp]") {
  std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};

  double mean = llvm_ml::stat::mean(values);

  REQUIRE_THAT(mean, Catch::Matchers::WithinAbs(3.0, 0.001));
}

TEST_CASE("Standard deviation is computed correctly", "[statistics/cov.hpp]") {
  std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};

  double mean = llvm_ml::stat::mean(values);
  double stdev = llvm_ml::stat::standard_deviation(values, mean);

  REQUIRE_THAT(stdev, Catch::Matchers::WithinAbs(1.4, 0.1));
}

TEST_CASE("Coefficient of variation", "[statistics/cov.hpp]") {
  std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};

  double cov = llvm_ml::stat::coefficient_of_variation(values);

  REQUIRE_THAT(cov, Catch::Matchers::WithinAbs(0.4714, 0.01));
}
