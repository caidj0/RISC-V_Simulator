#include "CPU.hpp"

#include <cassert>
#include <cstdint>

#include "utils.hpp"

CPU::CPU() : PC(), mem(), regs(), cycle_time(0), stage(0) {}

enum OpType { U, J, I, B, S, R };

OpType get_opType(uint8_t op) {
    switch (op) {
        case 0b0110111:
        case 0b0010111:
            return OpType::U;
        case 0b1101111:
            return OpType::J;
        case 0b1100111:
        case 0b0000011:
        case 0b0010011:
            return OpType::I;
        case 0b1100011:
            return OpType::B;
        case 0b0100011:
            return OpType::S;
        case 0b0110011:
            return OpType::R;
        default:
            assert(false);
    }
}

void CPU::fetch() {
    mem.address <= LAM(PC);
    mem.write <= LAM(false);
    mem.input <= LAM(0);
    regs.rd = LAM(0);
    regs.rd_data = LAM(0);
    regs.rs1 = LAM(0);
    regs.rs2 = LAM(0);

    PC <= LAM(PC + 4);  // RISC-V 的地址偏移是以当前指令，而不是下一条指令！
}

void CPU::decode() {
    full_instrction = mem;

    op = LAM(full_instrction & 0b1111111);
    subop = LAM((full_instrction >> 12) & 0b111);
    rd = LAM((full_instrction >> 7) & 0b11111);
    rs1 = LAM((full_instrction >> 15) & 0b11111);
    rs2 = LAM((full_instrction >> 20) & 0b11111);
    imm = [&]() {
        int32_t ret = 0;
        switch (get_opType(op)) {
            case U:
                ret |= full_instrction & 0xFFFFF000U;
                break;
            case J:
                ret |= full_instrction & 0x000FF000U;
                ret |= (full_instrction & 0x00100000U) >> 9;
                ret |= (full_instrction & 0x7FE00000U) >> 20;
                ret |= (full_instrction & 0x80000000U) >> 11;
                break;
            case B:
                ret |= (full_instrction & 0x00000080U) << 4;
                ret |= (full_instrction & 0x00000F00U) >> 7;
                ret |= (full_instrction & 0x7E000000U) >> 20;
                ret |= (full_instrction & 0x80000000U) >> 19;
                break;
            case I:
                ret |= (full_instrction & 0xFFF00000U) >> 20;
                break;
            case S:
                ret |= (full_instrction & 0x00000F80U) >> 7;
                ret |= (full_instrction & 0xFE000000U) >> 20;
                break;
            case R:
                break;
        }
        return ret;
    };

    shamt = LAM((full_instrction & 0x01F00000U) >> 20);
    variant_flag = LAM(full_instrction & 0x40000000U);
}

void CPU::step() {
    switch (stage) {
        case 0:
            fetch();
            break;
        case 1:
            decode();
            break;
    }

    cycle_time++;
    stage = (stage + 1) % 5;
}

void CPU::pullAndUpdate() {
    Updatable *const updatables[] = {&PC, &mem, &regs};

    for (auto &x : updatables) {
        x->pull();
    }

    for (auto &x : updatables) {
        x->update();
    }
}