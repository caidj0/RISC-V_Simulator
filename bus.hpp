#pragma once

#include <cstddef>
#include <cstdint>

struct RSBus {
    size_t reorder_index;
    size_t qj;
    size_t qk;

    // 对于 ALURS，vj vk 存储两个操作数
    // 对于 MemRS，vj 存储寄存器读取的地址，vk 存储要存的数据，imm 存储地址偏移
    uint32_t vj;
    uint32_t vk;
    uint8_t subop;
    bool variant_flag;
    uint32_t imm;
};

struct ALUBus {
    size_t reorder_index;
    uint8_t subop;
    bool variant_flag;
    uint32_t num_A;
    uint32_t num_B;
};

struct MemBus {
    size_t reorder_index;
    uint8_t write_mode;
    uint32_t address;
    uint32_t input;
};

struct PCBus {
    bool flag;
    uint32_t address;
    uint32_t offset;
};

struct CommonDataBus {
    size_t reorder_index;
    uint32_t data;
};

struct RegIssueBus {
    uint8_t rd;
    size_t reorder_index;
};

struct RegCommitBus {
    size_t reorder_index;
    uint32_t data;
};

struct RegValueBus {
    size_t q;
    uint32_t v;
};

template <typename T>
T BusSelect(const auto& container, const auto& mapF) {
    for (const auto& x : container) {
        T temp = mapF(x);

        if (temp.reorder_index != 0) {
            return temp;
        }
    }

    return T();
}