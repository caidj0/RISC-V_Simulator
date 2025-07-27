#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "bus.hpp"
#include "utils.hpp"

template <size_t DELAY>
class Memory : public Updatable, public CDBSource {
    std::map<uint32_t, uint8_t> mems;

    Reg<size_t> record_index;
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
    Wire<CommonDataBus> cdb;
    Wire<MemBus> read_bus;
    Wire<MemBus> write_bus;
    Wire<uint32_t> PC;
    Wire<bool> clear;

    Memory() {
        instruction <= LAM(get(PC));
        write_bus_reg <= LAM(write_bus);
        remain_delay <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            MemBus rb = read_bus;
            if (rb.record_index != 0) {
                return DELAY;
            }
            return remain_delay > 0 ? remain_delay - 1 : 0;
        };
        record_index <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            if (cdb.value().index == record_index) {
                return 0;
            }

            MemBus rb = read_bus;
            if (rb.record_index != 0 && record_index == 0) {
                return rb.record_index;
            }

            return record_index;
        };
        out <= [&]() {
            if (clear) {
                return 0;
            }

            MemBus rb = read_bus;
            if (rb.record_index != 0 && record_index == 0) {
                return get(rb.address);
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
        return remain_delay == 0 ? CommonDataBus{record_index, out}
                                 : CommonDataBus();
    }

    void pull() {
        record_index.pull();
        remain_delay.pull();
        instruction.pull();
        out.pull();
        write_bus_reg.pull();
    }

    void update() {
        record_index.update();
        remain_delay.update();
        instruction.update();
        out.update();
        write_bus_reg.update();

        MemBus wb = write_bus_reg;
        if (wb.record_index != 0) {
            mems[wb.address] = wb.input & 0xff;
            if (wb.write_mode & 0b011) {
                mems[wb.address + 1] = (wb.input >> 8) & 0xff;
                if (wb.write_mode == 0b010) {
                    mems[wb.address + 2] = (wb.input >> 16) & 0xff;
                    mems[wb.address + 3] = (wb.input >> 24) & 0xff;
                }
            }
        }
    }
};