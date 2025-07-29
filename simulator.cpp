#include <cstdio>

#include "CPU.hpp"

int main() {
#ifdef DEBUG
    freopen("../../testcases/tak.data", "r", stdin);
    // freopen("../../../troubleshooting/hanoi_tom.txt", "w", stdout);
#endif
    CPU cpu;
    uint8_t ret;
    while (!cpu.step(ret));
    printf("%d\n", ret);

    return 0;
}