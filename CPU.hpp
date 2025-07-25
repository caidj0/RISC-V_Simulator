#include <cstdint>

#include "ALU.hpp"
#include "ROB.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include "regs.hpp"
#include "utils.hpp"

class CPU {
    Reg<uint32_t> PC;
    Regs regs;
    Memory<2> mem;
    ALU alu;
    ReorderBuffer<> rob;

    uint64_t cycle_time;
    uint8_t stage;  // 取指、译码、执行、访存、写回

    uint32_t full_instruction;
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

    CommonDataBus CDBSelect();

   public:
    CPU();

    bool step(uint8_t *ret);
};