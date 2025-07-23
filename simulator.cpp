#include <cstdio>

#include "CPU.hpp"

int main() {
    // freopen("../../../testcases/pi.data", "r", stdin);

    CPU cpu;
    uint8_t ret;
    while (!cpu.step(&ret));
    printf("%d\n", ret);

    return 0;
}