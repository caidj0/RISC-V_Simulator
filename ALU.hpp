#include <cstdint>
#include <format>
#include <stdexcept>

#include "bus.hpp"
#include "utils.hpp"

class ALU : public Updatable, public CDBSource {
    Reg<size_t> record_index;
    Reg<uint32_t> out;

   public:
    Wire<ALUBus> bus;
    Wire<size_t> cdb_index;
    Wire<bool> clear;
    operator uint32_t() const { return out; }

    ALU() {
        record_index <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            if (cdb_index == record_index) {
                return 0;
            }

            ALUBus ab = bus;
            if (ab.record_index != 0) {
                if (record_index != 0) {
                    throw std::runtime_error(
                        "New ALU operation requested while the last operation "
                        "not "
                        "finished yet!");
                }
                return ab.record_index;
            }

            return record_index;
        };

        out <= [&]() -> uint32_t {
            if (clear) {
                return 0;
            }

            ALUBus ab = bus;
            if (ab.record_index == 0) {
                return 0;
            }
            switch (ab.subop) {
                case 0b000:
                    if (ab.variant_flag) {  // sub
                        return ab.num_A - ab.num_B;
                    } else {  // add
                        return ab.num_A + ab.num_B;
                    }
                    break;
                case 0b001:  // sll
                    return ab.num_A << (ab.num_B & 0b11111);
                    break;
                case 0b010:  // slt
                    return static_cast<int32_t>(ab.num_A) <
                           static_cast<int32_t>(ab.num_B);
                    break;
                case 0b011:  // sltu
                    return ab.num_A < ab.num_B;
                    break;
                case 0b100:  // xor
                    return ab.num_A ^ ab.num_B;
                    break;
                case 0b101:
                    if (ab.variant_flag) {  // sra
                        // C++20 起规定为算数右移
                        return static_cast<int32_t>(ab.num_A) >>
                               (ab.num_B & 0b11111);
                    } else {  // srl
                        return ab.num_A >> (ab.num_B & 0b11111);
                    }
                    break;
                case 0b110:  // or
                    return ab.num_A | ab.num_B;
                    break;
                case 0b111:
                    return ab.num_A & ab.num_B;
                    break;
                default:
                    throw std::runtime_error(
                        std::format("Unknown ALU operation code 0b{:03b}",
                                    uint8_t(ab.subop)));
                    break;
            }
        };
    }

    void pull() {
        record_index.pull();
        out.pull();
    }

    void update() {
        record_index.pull();
        out.update();
    }
};