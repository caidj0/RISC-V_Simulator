#include <cstdint>

#include "utils.hpp"

class Regs : public Updatable {
    Reg<uint32_t> regs[32];
    Reg<uint32_t> _rs1_data;
    Reg<uint32_t> _rs2_data;

   public:
    Wire<uint8_t> rs1;
    Wire<uint8_t> rs2;
    Wire<uint8_t> rd;
    Wire<uint32_t> rd_data;

    void pull() {
        _rs1_data.f = LAM(regs[rs1]);
        _rs2_data.f = LAM(regs[rs2]);

        _rs1_data.pull();
        _rs2_data.pull();

        for (int i = 0; i < 31; i++) {
            if (i == rd) {
                regs[i].f = LAM(rd_data);
            } else {
                regs[i].f = LAM(regs[i]);
            }

            if (i == 0) {
                regs[i].f = LAM(0);
            }

            regs[i].pull();
        }
    }
    void update() {
        _rs1_data.update();
        _rs2_data.update();

        for (auto &reg : regs) {
            reg.update();
        }
    }
    uint32_t rs1_data() const { return _rs1_data; }
    uint32_t rs2_data() const { return _rs2_data; }

    uint32_t direct_access(uint8_t reg_index) const { return regs[reg_index]; }
};