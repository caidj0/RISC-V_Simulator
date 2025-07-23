#include <cstdio>

#include "CPU.hpp"

int main() {
    freopen("../../../testcases/array_test1.data", "r", stdin);

    CPU cpu;
    uint8_t ret;
    while (!cpu.step(&ret));
    printf("The return value is: %d\n", ret);

    return 0;
}