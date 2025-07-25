#pragma once

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <tuple>

#include "regs.hpp"
#include "utils.hpp"

struct ROBItem {
    Reg<uint32_t> full_instruction;
    Reg<bool> ready;
    Reg<uint32_t> value;
    Reg<uint32_t> PC;
    Reg<bool> branched;

    bool is_jalr() const { return get_op(full_instruction) == 0b1100111U; }

    bool is_branch() const { return get_op(full_instruction) == 0b1100011U; }

    bool is_store() const { return get_op(full_instruction) == 0b0100011U; }

    bool is_mispredicted() const {
        if (!is_branch()) {
            return false;
        }

        uint8_t subop = get_subop(full_instruction);

        bool should_branch =
            (subop == 0b000 && value == 0) || (subop == 0b001 && value != 0) ||
            (subop == 0b100 && value) || (subop == 0b101 && !value) ||
            (subop == 0b110 && value) || (subop == 0b111 && !value);

        return branched == should_branch;
    }

    uint8_t rs1() const { return get_rs1(full_instruction); }
    uint8_t rs2() const { return get_rs2(full_instruction); }
    uint8_t imm() const { return get_imm(full_instruction); }
    uint8_t sub_op() const { return get_subop(full_instruction); }
    uint8_t rd() const { return get_rd(full_instruction); }
};

template <size_t length = 8>
class ReorderBuffer : public Updatable {
    Reg<size_t> head;  // 1-based
    Reg<size_t> tail;
    ROBItem items[length + 1];

    const Regs& regs;

    size_t index_inc(size_t index) { return index == length ? 1 : index + 1; }

   public:
    Wire<size_t> cdb_index;
    Wire<uint32_t> cdb_data;
    Wire<bool> add_instruction;
    Wire<bool> branched;
    Wire<uint32_t> full_instruction;
    Wire<uint32_t> PC;

    ReorderBuffer(const Regs& regs) : regs(regs) {
        static_assert(length > 1,
                      "The reorder buffer needs at least two elements long!");
        head = 1;
        tail = 1;
        head <= [&]() {
            if (fresh()) {
                return 1;
            }

            if (commit()) {
                return index_inc(head);
            }

            return head;
        };
        tail <= [&]() -> size_t {
            if (fresh()) {
                return 1;
            }

            if (add_instruction) {
                auto new_tail = next_index();
                if (new_tail == 0) {
                    throw std::runtime_error(
                        "There is no more space but instruction added!");
                }
                return new_tail;
            }
            return tail;
        };

        auto need_update = [&](size_t index) {
            return !fresh() && add_instruction && tail == index;
        };

        // 更新每个 item
        for (size_t i = 1; i <= length; i++) {
            items[i].full_instruction <= [&, i]() {
                if (need_update(i)) {
                    return full_instruction;
                }
                return items[i].full_instruction;
            };
            items[i].ready <= [&, i]() {
                if (need_update(i)) {
                    return false;
                }

                if (cdb_index == i) {
                    return true;
                }

                return items[i].ready;
            };
            items[i].PC <= [&, i]() {
                if (need_update(i)) {
                    return PC;
                }
                return items[i].PC;
            };
            items[i].branched <= [&, i]() {
                if (need_update(i)) {
                    return branched;
                }
                return items[i].branched;
            };
            items[i].value <= [&, i]() {
                if (cdb_index == i) {
                    return cdb_data;
                }
                return items[i].value;
            };
        }
    }

    // 返回 0 说明 ROB 已满
    size_t next_index() const {
        auto next = index_inc(tail);
        return next == head ? 0 : next;
    }

    bool commit() const {
        if (head == tail) {
            return false;
        }
        if (!items[head].ready) {
            return false;
        }
        return true;
    }

    bool fresh() const {
        if (commit()) {
            return items[head].is_jalr() || items[head].is_mispredicted();
        }
        return false;
    }

    std::tuple<bool, uint32_t, uint32_t> PCRelocate() const {
        if (commit()) {
            // jalr 指令和 b 指令
            if (items[head].is_jalr()) {
                return std::make_tuple(true, regs[items[head].rs1()],
                                       items.imm());
            }

            if (items[head].is_mispredicted()) {
                return std::make_tuple(true, items[head].PC,
                                       items[head].branched ? 4 : items.imm());
            }
        }

        return std::make_tuple(false, 0, 0);
    }

    // write_mode, address, input
    std::tuple<bool, uint8_t, uint32_t, uint32_t> store() const {
        if (commit()) {
            if (items[head].is_store()) {
                return std::make_tuple(true, items[head].subop(),
                                       items[head].value,
                                       regs[items[head].rs2()]);
            }
        }
        return std::make_tuple(false, 0, 0, 0);
    }

    void pull() {
        head.pull();
        tail.pull();

        for (auto& item : items) {
            item.full_instruction.pull();
            item.ready.pull();
            item.rd.pull();
            item.value.pull();
        }
    }

    void update() {
        head.update();
        tail.update();
        for (auto& item : items) {
            item.full_instruction.update();
            item.ready.update();
            item.rd.update();
            item.value.update();
        }
    }
};