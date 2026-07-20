#ifndef IMGUI_PAINTER_TEST_HARNESS_H
#define IMGUI_PAINTER_TEST_HARNESS_H

#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace ip_test {

using TestFunction = void (*)();

inline std::map<std::string, TestFunction> &registry() {
    static std::map<std::string, TestFunction> tests;
    return tests;
}

struct Registrar {
    Registrar(const char *name, TestFunction function) {
        if (!registry().emplace(name, function).second) {
            throw std::runtime_error(std::string("duplicate test selector: ") + name);
        }
    }
};

inline void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline int run(int argc, char **argv) {
    try {
        require(argc == 2, "expected one test selector or --list");
        const std::string selector = argv[1];
        if (selector == "--list") {
            for (const auto &test : registry()) {
                std::cout << test.first << '\n';
            }
            return 0;
        }
        const auto test = registry().find(selector);
        require(test != registry().end(), "unknown test selector");
        test->second();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

} // namespace ip_test

#define IP_TEST_CASE(function_name, selector)                                  \
    void function_name();                                                     \
    const ::ip_test::Registrar registrar_##function_name(selector,            \
                                                          &function_name);     \
    void function_name()

#endif
