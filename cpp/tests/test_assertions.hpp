#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// Thrown by skip() when a test cannot run because an optional fixture is not
// present. acceptance_main turns it into exit code 77, which CTest reports as a
// skip rather than a failure, so a bare checkout stays green.
struct skip_test {
    std::string reason;
};
inline void skip(const std::string& reason) {
    throw skip_test{reason};
}

inline void require(bool condition, const std::string& name) {
    if(!condition)
        throw std::runtime_error(name);
}
inline void near(double actual, double expected, double tolerance, const std::string& name) {
    require(std::abs(actual - expected) <= tolerance, name);
}
template <class Function>
void rejects(Function&& function, const std::string& name) {
    try {
        function();
    } catch(const std::exception&) {
        return;
    }
    throw std::runtime_error(name);
}
template <class Function>
int acceptance_main(Function&& function) {
    try {
        function();
        std::cout << "PASS\n";
        return 0;
    } catch(const skip_test& skipped) {
        std::cout << "SKIP: " << skipped.reason << '\n';
        return 77;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
