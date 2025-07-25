#include <cstddef>
#include <cstdint>

#include "utils.hpp"

class Regs : public Updatable {
    Reg<uint32_t> _regs[32];

   public:
    Wire<uint8_t> rd;
    Wire<uint32_t> rd_data;

    Regs() {
        _regs[0] <= LAM(0);

        for (int i = 1; i < 32; i++) {
            _regs[i] <=
                [&, i]() -> uint32_t { return rd == i ? rd_data : _regs[i]; };
        }
    }

    void pull() {
        for (auto &reg : _regs) {
            reg.pull();
        }
    }
    void update() {
        for (auto &reg : _regs) {
            reg.update();
        }
    }

    uint32_t operator[](uint8_t index) const { return _regs[index]; }
};

class RegisterStats : public Updatable {
    Reg<size_t> _reorder[32];

   public:
    Wire<uint8_t> rd;
    Wire<size_t> reorder;

    RegisterStats() {
        _reorder[0] <= LAM(0);
        for (int i = 1; i < 32; i++) {
            _reorder[i] <=
                [&, i]() -> uint8_t { return rd == i ? reorder : _reorder[i]; };
        }
    }

    void pull() {
        for (auto &x : _reorder) {
            x.pull();
        }
    }

    void update() {
        for (auto &x : _reorder) {
            x.update();
        }
    }

    size_t operator[](uint8_t index) const { return _reorder[index]; }
};