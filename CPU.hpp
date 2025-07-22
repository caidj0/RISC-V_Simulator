#include <cstdint>

#include "memory.hpp"
#include "regs.hpp"
#include "utils.hpp"

enum OpType { U, J, I, B, S, R };

class CPU {
    Reg<uint32_t> PC;
    Memory<> mem;
    Regs regs;

    uint64_t cycle_time;
    uint8_t stage;  // 取指、译码、执行、访存、写回

    uint32_t full_instrction;
    Wire<uint8_t> op;
    Wire<uint8_t> subop;
    Wire<uint32_t> imm;
    Wire<uint8_t> rs1;
    Wire<uint8_t> rs2;
    Wire<uint8_t> rd;
    Wire<uint8_t> shamt;
    Wire<bool> variant_flag;
    Wire<OpType> op_type;

    Reg<uint32_t> rA;
    Reg<uint32_t> rB;

    void fetch();
    void decode();
    void pullAndUpdate();

   public:
    CPU();

    void step();
};