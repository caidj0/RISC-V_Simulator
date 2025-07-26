#include "CPU.hpp"

#include <cstdint>

#include "bus.hpp"
#include "regs.hpp"
#include "utils.hpp"

CPU::CPU()
    : PC(),
      regs(),
      regstats(),
      rob(regs),
      mem(),
      mem_rs(rob),
      alus(),
      alu_rses{rob, rob},
      cycle_time(0),
      updatables(collectPointer<Updatable>(PC, regs, regstats, rob, mem, mem_rs,
                                           alus, alu_rses)),
      cdb_sources(collectPointer<CDBSource>(mem, alus)) {
    full_instruction = LAM(mem.get_instruction());
    valid_instruction <= [&]() -> bool { return !rob.clear(); };
}

bool CPU::step(uint8_t &ret) {
    pullAndUpdate();
    cycle_time++;
    return false;
}

void CPU::pullAndUpdate() {
    for (auto &x : updatables) {
        x->pull();
    }

    for (auto &x : updatables) {
        x->update();
    }
}

CommonDataBus CPU::CDBSelect() {
    CommonDataBus ret;
    for (const auto &x : cdb_sources) {
        auto temp = x->CDBOut();

        // 暂时优先选择编号小的
        if (ret.index == 0 || temp.index < ret.index) {
            ret = temp;
        }
    }

    return ret;
}