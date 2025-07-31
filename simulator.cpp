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
                     "Cycle time: {}; total branch num: {}, correct branch "
                     "count: {}, "
                     "correct ratio: {}; total jalr num: {}, correct jalr "
                     "count: {}, correct ratio: {}.",
                     cpu.cycleTime(), ps.total_branch, ps.correct_branch,
                     1.0 * ps.correct_branch / ps.total_branch, ps.total_jalr,
                     ps.correct_jalr, 1.0 * ps.correct_jalr / ps.total_jalr)
              << std::endl;
#endif

    return 0;
}