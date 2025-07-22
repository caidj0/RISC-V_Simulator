#include <cassert>
#include <cstdint>

#include "utils.hpp"

class ALU : public Updatable {
    Reg<uint32_t> out;

   public:
    Wire<uint32_t> num_A;
    Wire<uint32_t> num_B;
    Wire<uint8_t> op;  // 只使用低三位
    Wire<bool> variant_flag;
    operator uint32_t() const { return out; }

    void pull() {
        switch (op) {
            case 0b000:
                if (variant_flag) {  // sub
                    out <= LAM(num_A - num_B);
                } else {  // add
                    out <= LAM(num_A + num_B);
                }
                break;
            case 0b001:  // sll
                out <= LAM(num_A << (num_B & 0b11111));
                break;
            case 0b010:  // slt
                out <= LAM(int32_t(num_A) < int32_t(num_B));
                break;
            case 0b011:  // sltu
                out <= LAM(num_A < num_B);
                break;
            case 0b100:  // xor
                out <= LAM(num_A ^ num_B);
                break;
            case 0b101:
                if (variant_flag) {  // sra
                    // C++20 起规定为算数右移
                    out <= LAM(int32_t(num_A) >> (num_B & 0b11111));
                } else {  // srl
                    out <= LAM(num_A >> (num_B & 0b11111));
                }
                break;
            case 0b110:  // or
                out <= LAM(num_A | num_B);
                break;
            case 0b111:
                out <= LAM(num_A & num_B);
                break;
            default:
                assert(false);
                break;
        }

        out.pull();
    }

    void update() { out.update(); }
};