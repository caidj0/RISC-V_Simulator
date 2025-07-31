#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

#include "bus.hpp"
#include "utils.hpp"

class BaseMemory : public CDBSource {
   public:
    Wire<CommonDataBus> cdb;
    Wire<MemBus> read_bus;
    Wire<MemBus> write_bus;
    Wire<uint32_t> PC;
    Wire<bool> clear;

    virtual uint32_t get_instruction() const = 0;
};

template <size_t DELAY>
class Memory : public Updatable, public BaseMemory {
    std::map<uint32_t, uint8_t> mems;

    Reg<size_t> reorder_index;
    Reg<size_t> remain_delay;
    Reg<uint32_t> instruction;
    Reg<uint32_t> out;

    Reg<MemBus> write_bus_reg;

    uint32_t get(uint32_t address) {
        uint32_t ret = 0;
        ret = mems[address];
        ret |= mems[address + 1] << 8;
        ret |= mems[address + 2] << 16;
        ret |= mems[address + 3] << 24;
        return ret;
    }

   public:
    Memory() {
        instruction <= LAM(get(PC));
        write_bus_reg <= LAM(write_bus);
        remain_delay <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            MemBus rb = read_bus;
            if (rb.reorder_index != 0 && reorder_index == 0) {
                return DELAY;
            }
            return remain_delay > 0 ? remain_delay - 1 : 0;
        };
        reorder_index <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            if (reorder_index != 0) {
                if (cdb.value().reorder_index == reorder_index) {
                    return 0;
                }
            } else {
                MemBus rb = read_bus;
                if (rb.reorder_index != 0) {
                    return rb.reorder_index;
                }
            }

            return reorder_index;
        };
        out <= [&]() -> uint32_t {
            if (clear) {
                return 0;
            }

            MemBus rb = read_bus;
            if (rb.reorder_index != 0 && reorder_index == 0) {
                uint32_t got = get(rb.address);
                switch (rb.mode) {
                    case 0b000U:
                        return sext<8>(got & 0x000000FFU);
                    case 0b001U:
                        return sext<16>(got & 0x0000FFFFU);
                    case 0b010U:
                        return got;
                    case 0b100U:
                        return got & 0x000000FFU;
                    case 0b101U:
                        return got & 0x0000FFFFU;
                }
            }
            return out;
        };

        uint32_t address = 0;
        std::string buffer;
        while (!std::cin.eof()) {
            std::cin >> buffer;
            if (buffer.empty()) continue;

            if (buffer[0] == '@') {
                address = stoul(buffer.substr(1, buffer.size()), nullptr, 16);
            } else {
                uint8_t t = std::stoul(buffer, nullptr, 16);
                mems[address] = t;
                address++;
            }
        }
    }

    uint32_t get_instruction() const { return instruction; }

    CommonDataBus CDBOut() const {
        return remain_delay == 0 ? CommonDataBus{reorder_index, out}
                                 : CommonDataBus();
    }

    void pull() {
        reorder_index.pull();
        remain_delay.pull();
        instruction.pull();
        out.pull();
        write_bus_reg.pull();
    }

    void update() {
        reorder_index.update();
        remain_delay.update();
        instruction.update();
        out.update();
        write_bus_reg.update();

        MemBus wb = write_bus_reg;
        if (wb.reorder_index != 0) {
            mems[wb.address] = wb.input & 0xff;
            if (wb.mode & 0b011) {
                mems[wb.address + 1] = (wb.input >> 8) & 0xff;
                if (wb.mode == 0b010) {
                    mems[wb.address + 2] = (wb.input >> 16) & 0xff;
                    mems[wb.address + 3] = (wb.input >> 24) & 0xff;
                }
            }
        }
    }
};

template <size_t s, size_t E, size_t b, size_t CacheDelay, size_t MemoryDelay>
    requires(b > 0 && s + b <= 32 && E > 0)
class CacheMemory : public Updatable, public BaseMemory {
    constexpr static size_t S = 1 << s;
    constexpr static size_t B = 1 << b;
    constexpr static size_t t = 32 - s - b;

    struct CacheItem {
        bool valid;
        uint32_t mark;  // 只用它的后 t 位
        uint8_t data[B];

        uint32_t get(uint32_t lower_address, uint8_t mode) const {
            uint32_t ret = 0;
            ret = data[lower_address];
            if (mode & 0b011U) {
                ret |= data[lower_address + 1] << 8;
                if (mode & 0b010U) {
                    ret |= data[lower_address + 2] << 16;
                    ret |= data[lower_address + 3] << 24;
                }
            }
            return ret;
        }
    };

    struct CacheGroup {
        Reg<CacheItem> items[E];
    };

    std::map<uint32_t, uint8_t> mems;
    CacheGroup groups[S];

    Reg<MemBus> write_bus_reg;
    Reg<MemBus> read_bus_reg;
    Reg<uint32_t> instruction;
    Reg<size_t> remain_delay;

    std::mt19937 rng;
    std::uniform_int_distribution<> replace_selector;
    Reg<size_t> random_index;

    uint32_t direct_get(uint32_t address) {
        uint32_t ret = 0;
        ret = mems[address];
        ret |= mems[address + 1] << 8;
        ret |= mems[address + 2] << 16;
        ret |= mems[address + 3] << 24;
        return ret;
    }

    uint32_t getGroupIndex(uint32_t address) const {
        return (address >> b) & (S - 1);
    }

    uint32_t getMark(uint32_t address) const { return address >> (s + b); }

    uint32_t getLowerAddress(uint32_t address) const {
        return address & (B - 1);
    }

    // 当能找到时，返回 {true, index}；当找不到时，返回 {false, 用于替换的地址}
    std::pair<bool, size_t> findInGroup(uint32_t group_index,
                                        uint32_t mark) const {
        for (size_t item_index = 0; item_index < E; item_index++) {
            const CacheItem &item = groups[group_index].items[item_index];
            if (item.valid && item.mark == mark) {
                return {true, item_index};
            }
        }

        for (size_t item_index = 0; item_index < E; item_index++) {
            const CacheItem &item = groups[group_index].items[item_index];
            if (!item.valid) {
                return {false, item_index};
            }
        }

        return {false, random_index};
    }

    void load_data(uint32_t address, CacheItem &item) {
        uint32_t start_address = address & (~(B - 1));

        for (size_t i = 0; i < B; i++) {
            item.data[i] = mems[start_address + i];
        }
    }

    void checkBound(uint32_t lower_address, uint8_t mode) const {
        size_t offset = 0;
        if (mode & 0b010U) {
            offset = 3;
        } else if (mode & 0b001U) {
            offset = 1;
        }

        if (lower_address + offset >= B) {
            throw std::runtime_error(
                "Accessing data cross over different cache line is not "
                "allowed!");
        }
    }

   public:
    CacheMemory() : replace_selector(0, E - 1) {
        instruction <= LAM(direct_get(PC));
        write_bus_reg <= LAM(write_bus);
        random_index <= LAM(replace_selector(rng));
        read_bus_reg <= [&]() -> MemBus {
            CommonDataBus fetched_cdb = cdb;
            MemBus old_read_bus_reg = read_bus_reg;
            if (fetched_cdb.reorder_index != 0 &&
                fetched_cdb.reorder_index == old_read_bus_reg.reorder_index) {
                old_read_bus_reg.reorder_index = 0;
                return old_read_bus_reg;
            }

            if (old_read_bus_reg.reorder_index == 0) {
                return read_bus;
            }

            return read_bus_reg;
        };

        for (size_t group_index = 0; group_index < S; group_index++) {
            for (size_t item_index = 0; item_index < E; item_index++) {
                Reg<CacheItem> &item = groups[group_index].items[item_index];

                item <= [&, group_index, item_index]() -> CacheItem {
                    MemBus rb = read_bus;
                    MemBus wb = write_bus;
                    MemBus rbr = read_bus_reg;

                    CacheItem new_item = item;

                    if (rb.reorder_index != 0 && rbr.reorder_index == 0 &&
                        getGroupIndex(rb.address) == group_index) {
                        auto target_mark = getMark(rb.address);
                        auto result = findInGroup(group_index, target_mark);
                        if (!result.first && result.second == item_index) {
                            new_item.valid = true;
                            new_item.mark = target_mark;
                            load_data(rb.address, new_item);
                        }
                    }

                    if (new_item.valid && wb.reorder_index != 0 &&
                        getGroupIndex(wb.address) == group_index &&
                        getMark(wb.address) == new_item.mark) {
                        uint32_t lower_address = getLowerAddress(wb.address);
                        checkBound(lower_address, wb.mode);

                        new_item.data[lower_address] = wb.input & 0xff;
                        if (wb.mode & 0b011) {
                            new_item.data[lower_address + 1] =
                                (wb.input >> 8) & 0xff;
                            if (wb.mode == 0b010) {
                                new_item.data[lower_address + 2] =
                                    (wb.input >> 16) & 0xff;
                                new_item.data[lower_address + 3] =
                                    (wb.input >> 24) & 0xff;
                            }
                        }
                    }

                    return new_item;
                };
            }
        }

        remain_delay <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            MemBus rb = read_bus;
            MemBus rbr = read_bus_reg;
            if (rb.reorder_index != 0 && rbr.reorder_index == 0) {
                auto target_group_index = getGroupIndex(rb.address);
                auto target_mark = getMark(rb.address);

                return findInGroup(target_group_index, target_mark).first
                           ? CacheDelay
                           : MemoryDelay;
            }

            return remain_delay > 0 ? remain_delay - 1 : 0;
        };

        uint32_t address = 0;
        std::string buffer;
        while (!std::cin.eof()) {
            std::cin >> buffer;
            if (buffer.empty()) continue;

            if (buffer[0] == '@') {
                address = stoul(buffer.substr(1, buffer.size()), nullptr, 16);
            } else {
                uint8_t t = std::stoul(buffer, nullptr, 16);
                mems[address] = t;
                address++;
            }
        }
    }

    uint32_t get_instruction() const { return instruction; }

    CommonDataBus CDBOut() const {
        if (remain_delay == 0) {
            const MemBus &rbr = read_bus_reg;
            if (rbr.reorder_index != 0) {
                uint32_t lower_address = rbr.address & (B - 1);
                checkBound(lower_address, rbr.mode);

                auto target_group_index = getGroupIndex(rbr.address);
                auto target_mark = getMark(rbr.address);
                auto result = findInGroup(target_group_index, target_mark);

                assert(result.first);

                const CacheItem &item =
                    groups[target_group_index].items[result.second];
                uint32_t got = item.get(lower_address, rbr.mode);
                if (rbr.mode == 0b000U) {
                    got = sext<8>(got);
                } else if (rbr.mode == 0b001U) {
                    got = sext<16>(got);
                }

                return CommonDataBus{rbr.reorder_index, got};
            }
        }
        return CommonDataBus{};
    }

    void pull() {
        write_bus_reg.pull();
        read_bus_reg.pull();
        instruction.pull();
        remain_delay.pull();
        random_index.pull();

        for (auto &group : groups) {
            for (auto &item : group.items) {
                item.pull();
            }
        }
    }

    void update() {
        write_bus_reg.update();
        read_bus_reg.update();
        instruction.update();
        remain_delay.update();
        random_index.update();

        for (auto &group : groups) {
            for (auto &item : group.items) {
                item.update();
            }
        }

        MemBus wb = write_bus_reg;
        if (wb.reorder_index != 0) {
            mems[wb.address] = wb.input & 0xff;
            if (wb.mode & 0b011) {
                mems[wb.address + 1] = (wb.input >> 8) & 0xff;
                if (wb.mode == 0b010) {
                    mems[wb.address + 2] = (wb.input >> 16) & 0xff;
                    mems[wb.address + 3] = (wb.input >> 24) & 0xff;
                }
            }
        }
    }
};