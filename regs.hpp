#pragma once

#include <cstddef>
#include <cstdint>

#include "bus.hpp"
#include "utils.hpp"

class Regs : public Updatable {
    Reg<uint32_t> _regs[32];
    Reg<size_t> _reorder[32];

   public:
    Wire<RegIssueBus> issue_bus;
    Wire<RegCommitBus> commit_bus;
    Wire<bool> clear;

    Regs() {
        _regs[0] <= LAM(0);
        _reorder[0] <= LAM(0);

        for (uint8_t i = 1; i < 32; i++) {
            _regs[i] <= [&, i]() -> uint32_t {
                RegCommitBus cb = commit_bus;
                if (_reorder[i] == cb.reorder_index) {
                    return cb.data;
                }
                return _regs[i];
            };
            _reorder[i] <= [&, i]() -> size_t {
                if (clear) return 0;

                RegIssueBus ib = issue_bus;
                RegCommitBus cb = commit_bus;

                if (i == ib.rd) {
                    return ib.reorder_index;
                }

                if (_reorder[i] == cb.reorder_index) {
                    return 0;
                }

                return _reorder[i];
            };
        }
    }

    void pull() {
        for (auto &reg : _regs) {
            reg.pull();
        }
        for (auto &recorder : _reorder) {
            recorder.pull();
        }
    }
    void update() {
        for (auto &reg : _regs) {
            reg.update();
        }
        for (auto &recorder : _reorder) {
            recorder.update();
        }
    }

    uint32_t reg(uint8_t index) const { return _regs[index]; }

    uint8_t reorder(uint8_t index) const { return _reorder[index]; }
};