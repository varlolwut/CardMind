#pragma once

#include <string>

namespace cardputer {

struct CalculationResult {
    bool success;
    double value;
    std::string error;
};

CalculationResult calculateExpression(const std::string& expression);
std::string formatCalculationResult(double value);

}  // namespace cardputer
