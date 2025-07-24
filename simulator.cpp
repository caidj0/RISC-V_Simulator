#include <cstdio>

#include "CPU.hpp"

int main() {
#ifdef DEBUG
    freopen("../../testcases/naive.data", "r", stdin);
#endif
    CPU cpu;
    uint8_t ret;
    while (!cpu.step(&ret));
    printf("%d\n", ret);

    return 0;
}