#include <cstdio>
#include <iostream>

#include "utils.hpp"

int main() {
    int x = 100;
    const auto w = WIRE(x + x);
    x = 200;

    std::cout << w << std::endl;

    x = 1000;
    std::cout << w << std::endl;

    return 0;
}