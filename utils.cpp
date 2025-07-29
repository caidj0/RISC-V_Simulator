#include "utils.hpp"

#include <cstdint>

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
            return OpType::Unknown;
    }
}

uint8_t get_op(uint32_t full_instruction) {
    return full_instruction & 0b1111111U;
}

uint8_t get_subop(uint32_t full_instruction) {
    return (full_instruction >> 12) & 0b111;
}

uint8_t get_rs1(uint32_t full_instruction) {
    auto type = get_opType(get_op(full_instruction));
    if (type == U || type == J) {
        return 0;
    }

    return (full_instruction >> 15) & 0b11111;
}

uint8_t get_rs2(uint32_t full_instruction) {
    auto type = get_opType(get_op(full_instruction));
    if (type == B || type == S || type == R) {
        return (full_instruction >> 20) & 0b11111;
    }

    return 0;
}

uint8_t get_rd(uint32_t full_instruction) {
    auto op_type = get_opType(get_op(full_instruction));
    if (op_type != OpType::B && op_type != OpType::S) {
        return (full_instruction >> 7) & 0b11111;
    }
    return 0;
}

uint32_t get_imm(uint32_t full_instruction) {
    uint32_t ret = 0;
    switch (get_opType(get_op(full_instruction))) {
        case Unknown:
            break;
        case U:
            ret |= full_instruction & 0xFFFFF000U;
            break;
        case J:
            ret |= full_instruction & 0x000FF000U;
            ret |= (full_instruction & 0x00100000U) >> 9;
            ret |= (full_instruction & 0x7FE00000U) >> 20;
            ret |= (full_instruction & 0x80000000U) >> 11;
            ret = sext<21>(ret);
            break;
        case B:
            ret |= (full_instruction & 0x00000080U) << 4;
            ret |= (full_instruction & 0x00000F00U) >> 7;
            ret |= (full_instruction & 0x7E000000U) >> 20;
            ret |= (full_instruction & 0x80000000U) >> 19;
            ret = sext<13>(ret);
            break;
        case I:
            ret |= (full_instruction & 0xFFF00000U) >> 20;
            ret = sext<12>(ret);
            break;
        case S:
            ret |= (full_instruction & 0x00000F80U) >> 7;
            ret |= (full_instruction & 0xFE000000U) >> 20;
            ret = sext<12>(ret);
            break;
        case R:
            break;
    }
    return ret;
}

uint8_t get_shamt(uint32_t full_instruction) {
    return (full_instruction & 0x01F00000U) >> 20;
}

bool get_variant_flag(uint32_t full_instruction) {
    auto op = get_op(full_instruction);
    auto subop = get_subop(full_instruction);
    if (op == 0b1100011U) { /* Branch */
        return subop == 0b000 || subop == 0b001;
    }
    if ((op == 0b0010011U && subop == 0b101U) || op == 0b0110011U) {
        return full_instruction & 0x40000000U;
    }
    return false;
}