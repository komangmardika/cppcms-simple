#ifndef __TEST_FRAMEWORK_H__
#define __TEST_FRAMEWORK_H__

// A minimal test harness.
//
// The project vendors header-only dependencies rather than pulling in system
// packages, so rather than adding Catch2 or GoogleTest this provides just the
// pieces the person CRUD tests need: registration, assertions that report
// file and line, and a runner that keeps going after a failing test.
//
//   TEST(creates_a_person) {
//       CHECK_EQ(2 + 2, 4);
//   }

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace testing {

// Thrown by a failing assertion to abandon the current test without taking the
// rest of the suite down with it.
struct AssertionFailure : public std::runtime_error {
    explicit AssertionFailure(const std::string &message) : std::runtime_error(message) {}
};

typedef void (*TestFn)();

struct TestCase {
    std::string name;
    TestFn fn;
};

inline std::vector<TestCase> &registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char *name, TestFn fn)
    {
        TestCase test;
        test.name = name;
        test.fn = fn;
        registry().push_back(test);
    }
};

// Renders a value for an assertion message, quoting strings so an empty or
// space-padded value is visible in the output.
template <typename T>
std::string describe(const T &value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string describe(const std::string &value) { return "\"" + value + "\""; }
inline std::string describe(const char *value) { return std::string("\"") + value + "\""; }
inline std::string describe(bool value) { return value ? "true" : "false"; }

inline int run()
{
    const std::vector<TestCase> &tests = registry();
    size_t passed = 0;
    std::vector<std::string> failures;

    for (size_t i = 0; i < tests.size(); ++i) {
        try {
            tests[i].fn();
            std::cout << "  ok    " << tests[i].name << std::endl;
            ++passed;
        }
        catch (const AssertionFailure &e) {
            std::cout << "  FAIL  " << tests[i].name << std::endl
                      << "        " << e.what() << std::endl;
            failures.push_back(tests[i].name);
        }
        catch (const std::exception &e) {
            std::cout << "  ERROR " << tests[i].name << std::endl
                      << "        unexpected exception: " << e.what() << std::endl;
            failures.push_back(tests[i].name);
        }
    }

    std::cout << std::endl
              << passed << "/" << tests.size() << " tests passed" << std::endl;

    if (!failures.empty()) {
        std::cout << "failed:" << std::endl;
        for (size_t i = 0; i < failures.size(); ++i)
            std::cout << "  - " << failures[i] << std::endl;
        return 1;
    }

    return 0;
}

} // namespace testing

#define TEST(test_name)                                                        \
    static void test_name();                                                   \
    static testing::Registrar registrar_##test_name(#test_name, test_name);    \
    static void test_name()

#define TEST_FAIL(message)                                                     \
    do {                                                                       \
        std::ostringstream _msg;                                               \
        _msg << __FILE__ << ":" << __LINE__ << ": " << message;                \
        throw testing::AssertionFailure(_msg.str());                           \
    } while (false)

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition))                                                      \
            TEST_FAIL("expected " << #condition);                              \
    } while (false)

#define CHECK_FALSE(condition)                                                 \
    do {                                                                       \
        if (condition)                                                         \
            TEST_FAIL("expected !(" << #condition << ")");                     \
    } while (false)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        if (!((actual) == (expected)))                                         \
            TEST_FAIL("expected " << #actual << " == " << #expected            \
                      << ", got " << testing::describe(actual)                 \
                      << " vs " << testing::describe(expected));               \
    } while (false)

#endif // __TEST_FRAMEWORK_H__
