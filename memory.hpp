#include <cstdint>
#include <iostream>
#include <map>
#include <string>

#include "utils.hpp"

class Memory : public Updatable {
    uint32_t out;
    std::map<uint32_t, uint8_t> mems;

   public:
    Reg<uint32_t> address;
    Reg<bool> write;
    Reg<uint32_t> input;
    Reg<uint8_t> write_mode;

    operator uint32_t() const { return out; }
    void pull() {
        address.pull();
        write.pull();
        input.pull();
        write_mode.pull();
    }
    void update() {
        address.update();
        write.update();
        input.update();
        write_mode.update();
        if (write) {
            mems[address] = input & 0xff;
            if (write_mode & 0b011) {
                mems[address + 1] = (input >> 8) & 0xff;
                if (write_mode == 0b010) {
                    mems[address + 2] = (input >> 16) & 0xff;
                    mems[address + 3] = (input >> 24) & 0xff;
                }
            }
            out = 0;
        } else {
            out = mems[address];
            out |= mems[address + 1] << 8;
            out |= mems[address + 2] << 16;
            out |= mems[address + 3] << 24;
        }
    }

    Memory() {
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
};