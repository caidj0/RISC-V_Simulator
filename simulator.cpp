#include <cstdio>

#include "CPU.hpp"

int main() {
    freopen("sample.data", "r", stdin);

    CPU cpu;
    uint8_t ret;
    while (!cpu.step(&ret));
    printf("The return value is: %d", ret);

    return 0;
}