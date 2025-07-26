#include <cstdint>
#include <vector>

#include "ALU.hpp"
#include "ROB.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include "regs.hpp"
#include "rs.hpp"
#include "utils.hpp"

constexpr size_t N_ALU = 2;

class CPU {
    Reg<uint32_t> PC;
    Regs regs;
    RegisterStats regstats;
    ReorderBuffer<> rob;
    Memory<2> mem;
    ReservationStation<MemBus> mem_rs;
    ALU alus[N_ALU];
    ReservationStation<ALU> alu_rses[N_ALU];

    uint64_t cycle_time;

    Wire<uint32_t> full_instruction;
    Reg<bool> valid_instruction;

    const std::vector<Updatable*> updatables;
    const std::vector<CDBSource*> cdb_sources;

    void fetch();
    void decode();
    void execute();
    void memory();
    void writeBack();
    void pullAndUpdate();

    CommonDataBus CDBSelect();

   public:
    CPU();

    bool step(uint8_t &ret);
};