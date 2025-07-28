#include <cstddef>
#include <cstdint>
#include <vector>

#include "ALU.hpp"
#include "ROB.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include "predictor.hpp"
#include "regs.hpp"
#include "rs.hpp"
#include "utils.hpp"

// 1-based
constexpr size_t N_ALU = 4;
constexpr size_t N_MemRS = 4;

enum ExecuteType { None_T, ALU_T, Mem_T };

class CPU {
    Reg<uint32_t> PC;
    Regs regs;
    ReorderBuffer<> rob;
    Memory<2> mem;
    ReservationStation<MemBus> mem_rs[N_MemRS + 1];
    ALU alus[N_ALU + 1];
    ReservationStation<ALUBus> alu_rs[N_ALU + 1];
    Predictor predictor;

    Reg<uint64_t> cycle_time;

    Reg<uint32_t> instruction_PC;
    Wire<uint32_t> full_instruction;
    Reg<bool> valid_instruction;
    Wire<RSBus> rs_bus;
    Wire<ExecuteType> execute_type;
    Wire<size_t> rs_index;
    Wire<bool> issue;

    const std::vector<Updatable *> updatables;
    const std::vector<CDBSource *> cdb_sources;

    void pullAndUpdate();

    CommonDataBus CDBSelect() const;
    size_t MemRSSelect() const;
    size_t ALURSSelect() const;

    void PCInit();
    void baseWireInit();
    void regInit();
    void robInit();
    void memInit();
    void aluInit();
    void predictorInit();

   public:
    CPU();

    bool step(uint8_t &ret);
};