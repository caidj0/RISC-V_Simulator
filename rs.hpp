#pragma once

#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "ROB.hpp"
#include "bus.hpp"
#include "utils.hpp"

template <typename SpecBus>
class ReservationStation : public Updatable {
    Reg<RSBus> ins;

    bool is_ready() const { return RSBus(ins).qj == 0 && RSBus(ins).qk == 0; }

   public:
    Wire<CommonDataBus> cdb;
    Wire<RSBus> new_instruction;
    Wire<bool> clear;

    ReservationStation() {
        ins <= [&]() -> RSBus {
            if (clear) {
                return RSBus();
            }

            RSBus new_ins = new_instruction;
            RSBus old_ins = ins;
            if (new_ins.record_index != 0) {
                if (old_ins.record_index != 0) {
                    throw std::runtime_error(
                        "The requested reservation station is busy!");
                }
                return new_ins;
            }

            CommonDataBus local_cdb = cdb;

            if (local_cdb.index == old_ins.qj) {
                old_ins.vj = local_cdb.data;
                old_ins.qj = 0;
            }

            if (local_cdb.index == old_ins.qk) {
                old_ins.vk = local_cdb.data;
                old_ins.qk = 0;
            }

            if (local_cdb.index == old_ins.record_index) {
                old_ins.record_index = 0;
            }

            return old_ins;
        };
    }

    bool is_busy() const { return RSBus(ins).record_index != 0; }

    SpecBus execute(const ReorderBuffer<>& rob) const {
        if (!clear && is_busy() && is_ready()) {
            RSBus rsbus = ins;

            if constexpr (std::is_same<SpecBus, ALUBus>()) {
                return ALUBus{rsbus.record_index, rsbus.subop,
                              rsbus.variant_flag, rsbus.vj, rsbus.vk};
            } else if constexpr (std::is_same<SpecBus, MemBus>()) {
                uint32_t address = rsbus.vj + rsbus.imm;
                if (rob.canLoad(rsbus.record_index, address)) {
                    return MemBus{rsbus.record_index, rsbus.subop, address,
                                  rsbus.vk};
                }
            } else {
                static_assert(false, "Not supported type!");
            }
        }
        return SpecBus();
    }

    void pull() { ins.pull(); }

    void update() { ins.update(); }
};