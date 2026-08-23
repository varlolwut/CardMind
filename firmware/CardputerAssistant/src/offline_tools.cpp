#include "offline_tools.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace cardputer {
namespace {

struct ParseValue {
    bool success;
    double value;
    std::size_t position;
    std::string error;
};

std::size_t skipSpaces(const std::string& expression, std::size_t position)
{
    while (position < expression.size() && expression[position] == ' ') {
        ++position;
    }
    return position;
}

ParseValue parseSum(const std::string& expression, std::size_t position);

ParseValue parsePrimary(const std::string& expression, std::size_t position)
{
    const std::size_t start = skipSpaces(expression, position);
    if (start < expression.size() && expression[start] == '(') {
        const ParseValue inner = parseSum(expression, start + 1);
        if (!inner.success) {
            return inner;
        }
        const std::size_t closing = skipSpaces(expression, inner.position);
        if (closing >= expression.size() || expression[closing] != ')') {
            return {false, 0.0, closing, "Missing closing parenthesis"};
        }
        return {true, inner.value, closing + 1, ""};
    }

    std::size_t end = start;
    bool decimalSeen = false;
    while (end < expression.size()) {
        const char character = expression[end];
        if (character >= '0' && character <= '9') {
            ++end;
        } else if (character == '.' && !decimalSeen) {
            decimalSeen = true;
            ++end;
        } else {
            break;
        }
    }
    if (end == start || (end == start + 1 && expression[start] == '.')) {
        return {false, 0.0, start,
                "Expected a number at position " + std::to_string(start + 1)};
    }
    const std::string token = expression.substr(start, end - start);
    char* parsedEnd = nullptr;
    const double value = std::strtod(token.c_str(), &parsedEnd);
    if (parsedEnd == nullptr || *parsedEnd != '\0' || !std::isfinite(value)) {
        return {false, 0.0, start,
                "Invalid number at position " + std::to_string(start + 1)};
    }
    return {true, value, end, ""};
}

ParseValue parseUnary(const std::string& expression, std::size_t position)
{
    const std::size_t start = skipSpaces(expression, position);
    if (start < expression.size() && expression[start] == '+') {
        return parseUnary(expression, start + 1);
    }
    if (start < expression.size() && expression[start] == '-') {
        const ParseValue value = parseUnary(expression, start + 1);
        return value.success
            ? ParseValue{true, -value.value, value.position, ""}
            : value;
    }
    return parsePrimary(expression, start);
}

ParseValue parseProduct(const std::string& expression, std::size_t position)
{
    ParseValue result = parseUnary(expression, position);
    while (result.success) {
        const std::size_t operationPosition = skipSpaces(expression, result.position);
        if (operationPosition >= expression.size() ||
            (expression[operationPosition] != '*' && expression[operationPosition] != '/')) {
            return result;
        }
        const char operation = expression[operationPosition];
        const ParseValue right = parseUnary(expression, operationPosition + 1);
        if (!right.success) {
            return right;
        }
        if (operation == '/' && right.value == 0.0) {
            return {false, 0.0, operationPosition, "Division by zero"};
        }
        result = {true,
                  operation == '*' ? result.value * right.value : result.value / right.value,
                  right.position, ""};
    }
    return result;
}

ParseValue parseSum(const std::string& expression, std::size_t position)
{
    ParseValue result = parseProduct(expression, position);
    while (result.success) {
        const std::size_t operationPosition = skipSpaces(expression, result.position);
        if (operationPosition >= expression.size() ||
            (expression[operationPosition] != '+' && expression[operationPosition] != '-')) {
            return result;
        }
        const char operation = expression[operationPosition];
        const ParseValue right = parseProduct(expression, operationPosition + 1);
        if (!right.success) {
            return right;
        }
        result = {true,
                  operation == '+' ? result.value + right.value : result.value - right.value,
                  right.position, ""};
    }
    return result;
}

}  // namespace

CalculationResult calculateExpression(const std::string& expression)
{
    if (skipSpaces(expression, 0) >= expression.size()) {
        return {false, 0.0, "Expression is empty"};
    }
    const ParseValue result = parseSum(expression, 0);
    if (!result.success) {
        return {false, 0.0, result.error};
    }
    const std::size_t end = skipSpaces(expression, result.position);
    if (end != expression.size()) {
        return {false, 0.0,
                "Unexpected character at position " + std::to_string(end + 1)};
    }
    if (!std::isfinite(result.value)) {
        return {false, 0.0, "Calculation produced a non-finite result"};
    }
    return {true, result.value, ""};
}

std::string formatCalculationResult(double value)
{
    char buffer[40] = {};
    std::snprintf(buffer, sizeof(buffer), "%.10g", value);
    return buffer;
}

}  // namespace cardputer
