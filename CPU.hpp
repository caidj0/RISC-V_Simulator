#include <cstdint>

#include "memory.hpp"
#include "regs.hpp"
#include "utils.hpp"
#include "ALU.hpp"

enum OpType { U, J, I, B, S, R };

class CPU {
    Reg<uint32_t> PC;
    Memory mem;
    Regs regs;
    ALU alu;

    uint64_t cycle_time;
    uint8_t stage;  // 取指、译码、执行、访存、写回

    uint32_t full_instrction;
    Wire<uint8_t> op;
    Wire<uint8_t> subop;
    Wire<uint32_t> imm;
    Wire<uint8_t> op_rs1;
    Wire<uint8_t> op_rs2;
    Wire<uint8_t> op_rd;
    Wire<uint8_t> shamt;
    Wire<bool> variant_flag;
    Wire<OpType> op_type;

    void fetch();
    void decode();
    void execute();
    void memory();
    void writeBack();
    void pullAndUpdate();

   public:
    CPU();

    bool step(uint8_t *ret);
};