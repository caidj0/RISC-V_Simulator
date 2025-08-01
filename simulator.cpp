#include <cstdio>
#include <format>
#include <iostream>

#include "CPU.hpp"
#include "predictor.hpp"
#include "utils.hpp"

size_t wire_time = 1;

int main() {
    typedef CorrelatingPredictor<5, 5> Predictor1;
    typedef CorrelatingPredictor<0, 10> Predictor2;
    typedef TournamentPredictor<5, Predictor1, Predictor2> MixedPredictor;
    typedef CacheMemory<4, 4, 4, 0, 2> Cache;

    CPU<Predictor1, Cache, 8, 4, 4> cpu;
    uint8_t ret;
    while (!cpu.step(ret)) {
        wire_time++;
    }
    std::cout << +ret << std::endl;

#ifdef PROFILE
    auto ps = cpu.predictorStatistics();
    auto ms = cpu.memoryStatistics();
    std::cout << std::format(
                     "Cycle time: {};\ntotal branch num: {}, correct branch "
                     "count: {}, "
                     "correct ratio: {};\ntotal jalr num: {}, correct jalr "
                     "count: {}, correct ratio: {};\ntotal read count: {}, "
                     "cache hit count: "
                     "{}, ratio: {};\ntotal write count: {}.",
                     cpu.cycleTime(), ps.total_branch, ps.correct_branch,
                     1.0 * ps.correct_branch / ps.total_branch, ps.total_jalr,
                     ps.correct_jalr, 1.0 * ps.correct_jalr / ps.total_jalr,
                     ms.total_read_count, ms.read_cache_hit_count,
                     ms.read_cache_hit_count * 1.0 / ms.total_read_count,
                     ms.total_write_count)
              << std::endl;
#endif

    return 0;
}