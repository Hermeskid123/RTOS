/**
 * @file
 * @brief Defines TestFramework coverage for the RTOS framework test suite.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

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

/** @brief Registered test case name and executable body. */
struct Case {
    /** Human-readable test name. */
    std::string_view name;
    /** Test body that throws on failure. */
    std::function<void()> function;
};

/** @brief Returns the process-wide ordered test registry. */
inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

/** @brief Static helper that inserts a test case during initialization. */
class Registration {
public:
    /** @brief Registers `function` under `name`. */
    Registration(const std::string_view name, std::function<void()> function)
    {
        registry().push_back(Case{name, std::move(function)});
    }
};

/** @brief Throws a formatted assertion failure at the source location. */
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

/** @brief Runs every registered test and returns a process exit status. */
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

/** @brief Runs one exact named test and returns a process exit status. */
inline int runNamed(const std::string_view requestedName)
{
    for (const auto& testCase : registry()) {
        if (testCase.name != requestedName) {
            continue;
        }

        try {
            testCase.function();
            std::cout << "[PASS] " << testCase.name << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << testCase.name << ": " << error.what() << '\n';
            return 1;
        } catch (...) {
            std::cerr << "[FAIL] " << testCase.name << ": unknown exception\n";
            return 1;
        }
    }

    std::cerr << "Unknown test case: " << requestedName << '\n';
    return 2;
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
