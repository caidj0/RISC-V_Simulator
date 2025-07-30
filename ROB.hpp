#pragma once

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "bus.hpp"
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

        return branched != should_branch;
    }

    uint8_t rs1() const { return get_rs1(full_instruction); }
    uint8_t rs2() const { return get_rs2(full_instruction); }
    uint32_t imm() const { return get_imm(full_instruction); }
    uint8_t subop() const { return get_subop(full_instruction); }
    uint8_t rd() const { return get_rd(full_instruction); }
};

template <size_t length = 8>
class ReorderBuffer : public Updatable {
    Reg<size_t> head;  // 1-based
    Reg<size_t> tail;
    ROBItem items[length + 1];

    const Regs& regs;

    size_t index_inc(size_t index) const {
        return index == length ? 1 : index + 1;
    }

   public:
    Wire<CommonDataBus> cdb;
    Wire<bool> add_instruction;
    Wire<bool> branched;
    Wire<uint32_t> full_instruction;
    Wire<uint32_t> PC;

    ReorderBuffer(const Regs& regs) : regs(regs) {
        static_assert(length > 1,
                      "The reorder buffer needs at least two elements long!");
        head = 1;
        tail = 1;
        head <= [&]() -> size_t {
            if (clear()) {
                return 1;
            }

            if (commit()) {
                return index_inc(head);
            }

            return head;
        };
        tail <= [&]() -> size_t {
            if (clear()) {
                return 1;
            }

            if (add_instruction) {
                auto new_tail = index_inc(tail);
                if (new_tail == head) {
                    throw std::runtime_error(
                        "There is no more space but instruction added!");
                }
                return new_tail;
            }
            return tail;
        };

        auto need_update = [&](size_t index) {
            return !clear() && add_instruction && tail == index;
        };

        // 更新每个 item
        for (size_t i = 1; i <= length; i++) {
            items[i].full_instruction <= [&, i, need_update]() -> uint32_t {
                if (need_update(i)) {
                    return full_instruction;
                }
                return items[i].full_instruction;
            };
            items[i].ready <= [&, i, need_update]() -> bool {
                if (need_update(i)) {
                    return get_op(full_instruction) ==
                           0b0110111U;  // lui 指令已经 ready 了
                }

                if (cdb.value().reorder_index == i) {
                    return true;
                }

                return items[i].ready;
            };
            items[i].PC <= [&, i, need_update]() -> uint32_t {
                if (need_update(i)) {
                    return PC;
                }
                return items[i].PC;
            };
            items[i].branched <= [&, i, need_update]() -> bool {
                if (need_update(i)) {
                    return branched;
                }
                return items[i].branched;
            };
            items[i].value <= [&, i, need_update]() -> uint32_t {
                if (need_update(i) && get_op(full_instruction) == 0b0110111U) {
                    return get_imm(full_instruction);
                }

                CommonDataBus local_cdb = cdb;
                if (local_cdb.reorder_index == i) {
                    return local_cdb.data;
                }
                return items[i].value;
            };
        }
    }

    // 返回 0 说明 ROB 已满
    size_t get_index() const {
        auto next = index_inc(tail);
        return next == head ? 0 : tail;
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

    bool clear() const {
        if (commit()) {
            return jalr_mispredicted() || items[head].is_mispredicted();
        }
        return false;
    }

    bool canLoad(size_t query_index, uint32_t load_address) const {
        auto overlap = [](uint32_t address1, uint32_t address2) {
            if (address1 < address2) {
                return address2 - address1 < 4;
            } else {
                return address1 - address2 < 4;
            }
        };

        size_t current = head;
        while (current != query_index) {
            const auto& item = items[current];
            if (item.is_store()) {
                if (!item.ready || overlap(load_address, item.value)) {
                    return false;
                }
            }
            current = index_inc(current);
        }

        return true;
    }

    PCBus PCRelocate() const {
        if (commit()) {
            // jalr 指令和 b 指令
            if (jalr_mispredicted()) {
                return PCBus{true, regs.reg(items[head].rs1()),
                             items[head].imm()};
            }

            if (items[head].is_mispredicted()) {
                return PCBus{true, items[head].PC,
                             items[head].branched ? 4U : items[head].imm()};
            }
        }

        return PCBus();
    }

    bool jalr_mispredicted() const {
        if (!items[head].is_jalr()) {
            return false;
        }

        auto head_next = index_inc(head);
        if (head_next == tail) {
            return true;
        }

        uint32_t jaled_PC = items[head_next].PC;
        uint32_t expected_PC = regs.reg(items[head].rs1()) + items[head].imm();

        return jaled_PC != expected_PC;
    }

    MemBus store() const {
        if (commit() && !clear()) {
            if (items[head].is_store()) {
                return MemBus{head, items[head].subop(), items[head].value,
                              regs.reg(items[head].rs2())};
            }
        }
        return MemBus();
    }

    RegCommitBus regCommit() const {
        if (commit() && !clear()) {
            return RegCommitBus{head, items[head].rd(), items[head].value};
        }
        return RegCommitBus();
    }

    PredictFeedbackBus predictFeedback() const {
        if (commit() && items[head].is_branch()) {
            return PredictFeedbackBus{true, items[head].branched,
                                      items[head].is_mispredicted(),
                                      items[head].PC};
        }

        return PredictFeedbackBus{};
    }

    void pull() {
        head.pull();
        tail.pull();

        for (auto& item : items) {
            item.full_instruction.pull();
            item.ready.pull();
            item.PC.pull();
            item.value.pull();
            item.branched.pull();
        }
    }

    void update() {
        head.update();
        tail.update();
        for (auto& item : items) {
            item.full_instruction.update();
            item.ready.update();
            item.PC.update();
            item.value.update();
            item.branched.update();
        }
    }

    const ROBItem& front() const { return items[head]; }

    const ROBItem& getItem(size_t index) const {
        if (index == 0 || index > length) {
            throw std::out_of_range(std::format(
                "Accessing the {}th item out of {}!\n", index, length));
        }

        return items[index];
    }
};