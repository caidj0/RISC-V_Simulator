#include "CPU.hpp"

#include <cassert>
#include <cstdint>

#include "utils.hpp"

CPU::CPU() : PC(), mem(), regs(), cycle_time(0), stage(0) {}

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
    PC <= LAM(PC);  // RISC-V 的地址偏移是以当前指令，而不是下一条指令！
}

void CPU::decode() {
    full_instrction = mem;

    op = LAM(full_instrction & 0b1111111);
    subop = LAM((full_instrction >> 12) & 0b111);
    op_rd = LAM((full_instrction >> 7) & 0b11111);
    op_rs1 = LAM((full_instrction >> 15) & 0b11111);
    op_rs2 = LAM((full_instrction >> 20) & 0b11111);
    op_type = LAM(get_opType(op));
    imm = [&]() {
        uint32_t ret = 0;
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

    regs.rs1 = [&]() -> uint8_t {
        if (op_type != U && op_type != J) {
            return op_rs1;
        } else {
            return 0;
        }
    };
    regs.rs2 = [&]() -> uint8_t {
        switch (op_type) {
            case B:
            case S:
            case R:
                return op_rs2;
                break;
            case U:
            case J:
            case I:
                return 0;
                break;
        }
        return 0;
    };
    regs.rd = LAM(0);
    regs.rd_data = LAM(0);
}

void CPU::execute() {
    alu.num_A = [&]() -> uint32_t {
        switch (op_type) {
            case U:
                return op == 0b0110111 ? 0 : PC;
                break;
            case J:
                return PC;
                break;
            case I:
                return op == 0b1100111 ? PC : regs.rs1_data();
                break;
            case S:
            case B:
            case R:
                return regs.rs1_data();
                break;
        }
        return 0;
    };

    alu.num_B = [&]() -> uint32_t {
        switch (op_type) {
            case U:
                return imm;
                break;
            case J:
                return 4;
                break;
            case I:
                if (op == 0b1100111) {
                    return 4;
                } else {
                    if (alu.op == 0b001 || alu.op == 0b101) {
                        return shamt;
                    } else {
                        return sext<12>(imm);
                    }
                }
                break;
            case S:
                return sext<12>(imm);
                break;
            case B:
            case R:
                return regs.rs2_data();
                break;
        }
        return 0;
    };

    alu.op = [&]() -> uint8_t {
        if (op_type == B) {
            switch (subop) {
                case 0b000:
                case 0b001:
                    return 0b000;  // sub
                    break;
                case 0b100:
                case 0b101:
                    return 0b010;  // slt
                    break;
                case 0b110:
                case 0b111:
                    return 0b011;  // sltu
                    break;
                default:
                    return 0b000;
            }
        } else if (op_type == U || op_type == J || op_type == S ||
                   op == 0b0000011) {
            return 0b000;
        } else {
            return subop;
        }
    };

    alu.variant_flag = [&]() -> bool {
        if (op_type == B) {
            return subop == 0b000 || subop == 0b001;
        } else if (op == 0b0110011 || (op == 0b0010011 && subop == 0b101)) {
            return variant_flag;
        }
        return false;
    };
}

void CPU::memory() {
    mem.address <= LAM(alu);
    if (op_type == S) {
        mem.write_mode <= LAM(subop);
        mem.write <= LAM(true);
        mem.input <= LAM(regs.rs2_data());
    }
}

void CPU::writeBack() {
    mem.write <= LAM(false);
    regs.rd = [&]() -> uint8_t {
        return ((op_type == B) || op_type == S) ? 0 : op_rd;
    };
    regs.rd_data = [&]() -> uint32_t {
        if (op == 0b0000011) {
            switch (subop) {
                case 0b000:
                    return sext<8>(mem);
                    break;
                case 0b001:
                    return sext<16>(mem);
                    break;
                case 0b010:
                    return mem;
                    break;
                case 0b100:
                    return mem & 0x000000FFU;
                    break;
                case 0b101:
                    return mem & 0x0000FFFFU;
                    break;
                default:
                    assert(false);
            }
        } else {
            return alu;
        }
    };
    PC <= [&]() -> uint32_t {
        uint32_t offset = 4;
        switch (op_type) {
            case J:
                offset = sext<21>(imm);
                break;
            case I:
                if (op == 0b1100111) {
                    return (regs.rs1_data() + sext<12>(imm)) & 0xFFFFFFFEU;
                }
                break;
            case B:
                if ((subop == 0b000 && alu == 0) ||
                    (subop == 0b001 && alu != 0) || (subop == 0b100 && alu) ||
                    (subop == 0b101 && !alu) || (subop == 0b110 && alu) ||
                    (subop == 0b111 && !alu)) {
                    offset = sext<13>(imm);
                }
                break;
            case U:
            case R:
            case S:
                break;
        };
        return PC + offset;
    };
}

bool CPU::step(uint8_t *ret) {
    switch (stage) {
        case 0:
            fetch();
            break;
        case 1:
            decode();
            if (full_instrction == 0x0ff00513) {
                *ret = static_cast<uint8_t>(regs.direct_access(10));
                return true;
            }
            break;
        case 2:
            execute();
            break;
        case 3:
            memory();
            break;
        case 4:
            writeBack();
            break;
    }

    pullAndUpdate();
    cycle_time++;
    stage = (stage + 1) % 5;
    return false;
}

void CPU::pullAndUpdate() {
    Updatable *const updatables[] = {&PC, &mem, &regs, &alu};

    for (auto &x : updatables) {
        x->pull();
    }

    for (auto &x : updatables) {
        x->update();
    }
}