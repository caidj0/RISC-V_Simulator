#include <cstdint>
#include <format>
#include <stdexcept>

#include "bus.hpp"
#include "utils.hpp"

class ALU : public Updatable, public CDBSource {
    Reg<size_t> reorder_index;
    Reg<uint32_t> out;

   public:
    Wire<ALUBus> bus;
    Wire<CommonDataBus> cdb;
    Wire<bool> clear;

    CommonDataBus CDBOut() const { return CommonDataBus{reorder_index, out}; }

    ALU() {
        reorder_index <= [&]() -> size_t {
            if (clear) {
                return 0;
            }

            if (reorder_index != 0) {
                if (cdb.value().reorder_index == reorder_index) {
                    return 0;
                }
            } else {
                ALUBus ab = bus;
                if (ab.reorder_index != 0) {
                    return ab.reorder_index;
                }
            }

            return reorder_index;
        };

        out <= [&]() -> uint32_t {
            if (clear) {
                return 0;
            }

            ALUBus ab = bus;
            if (ab.reorder_index != 0 && reorder_index == 0) {
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
            }

            return out;
        };
    }

    void pull() {
        reorder_index.pull();
        out.pull();
    }

    void update() {
        reorder_index.update();
        out.update();
    }
};