#include <cstdio>
#include <format>
#include <iostream>

#include "CPU.hpp"
#include "predictor.hpp"

int main() {
    CPU<CorrelatingPredictor<5, 2>, 8, 4, 2, 4> cpu;
    uint8_t ret;
    while (!cpu.step(ret));
    std::cout << +ret << std::endl;

#ifdef PROFILE
    auto ps = cpu.predictorStatistics();
    std::cout << std::format(
                     "Cycle time: {}, total branch num: {}, correct predict "
                     "count: {}, "
                     "correct ratio: {}",
                     cpu.cycleTime(), ps.first, ps.second,
                     1.0 * ps.second / ps.first)
              << std::endl;
#endif

    return 0;
}