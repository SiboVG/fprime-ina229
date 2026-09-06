#ifndef INA229_TESTS_TEST_HARNESS_HPP
#define INA229_TESTS_TEST_HARNESS_HPP

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Ina229Test {

typedef void (*TestFunction)();

struct TestCase {
    const char *name;
    TestFunction function;
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char *name, TestFunction function) {
        registry().push_back(TestCase{name, function});
    }
};

inline void fail(const char *file, int line, const std::string &message) {
    std::ostringstream stream;
    stream << file << ':' << line << ": " << message;
    throw std::runtime_error(stream.str());
}

template <typename Expected, typename Actual>
inline void expectEqual(const Expected &expected, const Actual &actual, const char *expectedText,
                        const char *actualText, const char *file, int line) {
    if (!(expected == actual)) {
        std::ostringstream stream;
        stream << "expected " << expectedText << " == " << actualText;
        fail(file, line, stream.str());
    }
}

inline int runAll() {
    std::size_t failed = 0;
    for (std::size_t i = 0; i < registry().size(); ++i) {
        try {
            registry()[i].function();
            std::cout << "[PASS] " << registry()[i].name << '\n';
        } catch (const std::exception &error) {
            ++failed;
            std::cerr << "[FAIL] " << registry()[i].name << ": " << error.what() << '\n';
        }
    }
    std::cout << registry().size() - failed << '/' << registry().size() << " tests passed\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace Ina229Test

#define INA229_TEST(name)                                                                                    \
    static void name();                                                                                      \
    static Ina229Test::Registrar name##_registrar(#name, &name);                                             \
    static void name()

#define EXPECT_TRUE(condition)                                                                               \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            Ina229Test::fail(__FILE__, __LINE__, "expected true: " #condition);                              \
        }                                                                                                    \
    } while (false)

#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))

#define EXPECT_EQ(expected, actual)                                                                          \
    Ina229Test::expectEqual((expected), (actual), #expected, #actual, __FILE__, __LINE__)

#define EXPECT_NEAR(expected, actual, tolerance)                                                             \
    do {                                                                                                     \
        if (std::fabs((expected) - (actual)) > (tolerance)) {                                                \
            Ina229Test::fail(__FILE__, __LINE__, "values differ beyond tolerance");                          \
        }                                                                                                    \
    } while (false)

#endif
