#include <cstdint>

#include "utils.hpp"

class Regs : public Updatable {
    Reg<uint32_t> _regs[32];
    Reg<uint32_t> _rs1_data;
    Reg<uint32_t> _rs2_data;

   public:
    Wire<uint8_t> rs1;
    Wire<uint8_t> rs2;
    Wire<uint8_t> rd;
    Wire<uint32_t> rd_data;

    Regs() {
        _rs1_data <= LAM(_regs[rs1]);
        _rs2_data <= LAM(_regs[rs2]);

        _regs[0] <= LAM(0);

        for (int i = 1; i < 32; i++) {
            _regs[i] <=
                [&, i]() -> uint32_t { return rd == i ? rd_data : _regs[i]; };
        }
    }

    void pull() {
        _rs1_data.pull();
        _rs2_data.pull();
        for (auto &reg : _regs) {
            reg.pull();
        }
    }
    void update() {
        _rs1_data.update();
        _rs2_data.update();

        for (auto &reg : _regs) {
            reg.update();
        }
    }

    uint32_t rs1_data() const { return _rs1_data; }
    uint32_t rs2_data() const { return _rs2_data; }
    uint32_t direct_access(uint8_t reg_index) const { return _regs[reg_index]; }
};

class RegisterStats : public Updatable {
    // 此处使用 uint8_t，允许长度为 256 的 ROB
    Reg<uint8_t> _reorder[32];
    Reg<uint8_t> _rs1_reorder;
    Reg<uint8_t> _rs2_reorder;

   public:
    Wire<uint8_t> rs1;
    Wire<uint8_t> rs2;
    Wire<uint8_t> rd;
    Wire<uint8_t> reorder;

    RegisterStats() {
        _rs1_reorder <= LAM(_reorder[rs1]);
        _rs2_reorder <= LAM(_reorder[rs2]);
        _reorder[0] <= LAM(0);
        for (int i = 1; i < 32; i++) {
            _reorder[i] <=
                [&, i]() -> uint8_t { return rd == i ? reorder : _reorder[i]; };
        }
    }

    void pull() {
        _rs1_reorder.pull();
        _rs2_reorder.pull();
        for (auto &x : _reorder) {
            x.pull();
        }
    }

    void update() {
        _rs1_reorder.update();
        _rs2_reorder.update();
        for (auto &x : _reorder) {
            x.update();
        }
    }

    uint8_t rs1_reorder() const { return _rs1_reorder; }
    uint8_t rs2_reorder() const { return _rs2_reorder; }
};