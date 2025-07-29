#include <cstdio>

#include "CPU.hpp"
#include "predictor.hpp"

int main() {
    CPU<AlwaysBranchPredictor> cpu;
    uint8_t ret;
    while (!cpu.step(ret));
    printf("%d\n", ret);

    return 0;
}