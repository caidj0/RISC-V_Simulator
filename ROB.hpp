#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "utils.hpp"

struct ROBItem {
    Reg<uint32_t> full_instruction;
    Reg<bool> ready;
    Reg<uint8_t> rd;
    Reg<uint32_t> value;
};


template <size_t length = 8>
class ReorderBuffer : public Updatable {
    Reg<size_t> head;  // 1-based
    Reg<size_t> tail;

    ROBItem items[length + 1];

    size_t index_inc(size_t index) { return index == length ? 1 : index + 1; }

   public:
    Wire<size_t> cdb_index;
    Wire<uint32_t> cdb_data;
    Wire<bool> add_instruction;
    Wire<uint32_t> full_instruction;
    Wire<uint8_t> rd;

    ReorderBuffer() {
        static_assert(length > 1,
                      "The reorder buffer needs at least two elements long!");
        head = 1;
        tail = 2;
        head <= []() {
            // TODO
        };
        tail <= [&]() -> size_t {
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

        // 更新每个 item
        for (size_t i = 1; i <= length; i++) {
            items[i].full_instruction <= [&, i]() {
                if (add_instruction && tail == i) {
                    return full_instruction;
                }
                return items[i].full_instruction;
            };
            items[i].ready <= [&, i]() {
                if (add_instruction && tail == i) {
                    return false;
                }

                if (cdb_index == i) {
                    return true;
                }

                return items[i].ready;
            };
            items[i].rd <= [&, i]() {
                if (add_instruction && tail == i) {
                    return rd;
                }

                return items[i].rd;
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
    size_t next_index() { return tail == head ? 0 : tail; }

    void pull() {
        head.pull();
        tail.pull();

        for (auto &item : items) {
            item.full_instruction.pull();
            item.ready.pull();
            item.rd.pull();
            item.value.pull();
        }
    }

    void update() {
        head.update();
        tail.update();
        for (auto &item : items) {
            item.full_instruction.update();
            item.ready.update();
            item.rd.update();
            item.value.update();
        }
    }
};