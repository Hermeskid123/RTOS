#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test {

struct Case {
    std::string_view name;
    std::function<void()> function;
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

class Registration {
public:
    Registration(const std::string_view name, std::function<void()> function)
    {
        registry().push_back(Case{name, std::move(function)});
    }
};

[[noreturn]] inline void fail(
    const std::string_view expression,
    const std::string_view file,
    const int line
)
{
    std::ostringstream message;
    message << file << ':' << line << ": requirement failed: " << expression;
    throw std::runtime_error{message.str()};
}

inline int runAll()
{
    std::size_t failures{};

    for (const auto& testCase : registry()) {
        try {
            testCase.function();
            std::cout << "[PASS] " << testCase.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << testCase.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << testCase.name << ": unknown exception\n";
        }
    }

    std::cout << registry().size() - failures << '/' << registry().size()
              << " tests passed\n";
    return failures == 0 ? 0 : 1;
}

}  // namespace test

#define RTOS_TEST_JOIN_IMPL(left, right) left##right
#define RTOS_TEST_JOIN(left, right) RTOS_TEST_JOIN_IMPL(left, right)

#define TEST_CASE(name)                                                              \
    static void RTOS_TEST_JOIN(testFunction_, __LINE__)();                           \
    static const test::Registration RTOS_TEST_JOIN(testRegistration_, __LINE__){     \
        name, RTOS_TEST_JOIN(testFunction_, __LINE__)};                              \
    static void RTOS_TEST_JOIN(testFunction_, __LINE__)()

#define REQUIRE(expression)                                                          \
    do {                                                                             \
        if (!(expression)) {                                                         \
            test::fail(#expression, __FILE__, __LINE__);                             \
        }                                                                            \
    } while (false)
