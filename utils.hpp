#pragma once

#include <cstdint>
#include <format>
#include <functional>
#include <stdexcept>

template <typename T>
class Wire {
   public:
    std::function<T(void)> f;
    Wire() : f([]() { return T(); }) {}
    Wire(std::function<T(void)> f) : f(f) {}
    operator T() const { return f(); }
    Wire& operator=(std::function<T(void)> f) {
        this->f = f;
        return *this;
    }
    T value() const { return f(); }
};

class Updatable {
   public:
    virtual void pull() = 0;
    virtual void update() = 0;
};

template <typename T>
class Reg : public Updatable {
    T value;
    T new_value;

   public:
    std::function<T(void)> f;
    Reg(std::function<T(void)> f) : value(), new_value(), f(f) {}
    Reg() : Reg([]() { return T(); }) {}
    operator T() const { return value; }
    void pull() { new_value = f(); }
    void update() { value = new_value; }
    Reg& operator<=(const std::function<T(void)> f) {
        this->f = f;
        return *this;
    }
    Reg& operator=(const T value) {
        this->value = value;
        return *this;
    };
};

#define LAM(expr) [&]() { return (expr); }

// 符号拓展函数
template <uint32_t bits>
int32_t sext(uint32_t num) {
    struct signed_bit {
        int32_t val : bits;
    };
    return signed_bit{static_cast<int32_t>(num)}.val;
}

enum OpType { U, J, I, B, S, R };

OpType get_opType(uint8_t op);
uint8_t get_op(uint32_t full_instruction);
uint8_t get_subop(uint32_t full_instruction);
uint8_t get_rs1(uint32_t full_instruction);
uint8_t get_rs2(uint32_t full_instruction);
uint8_t get_rd(uint32_t full_instruction);
uint32_t get_imm(uint32_t full_instruction);
uint8_t get_shamt(uint32_t full_instruction);
bool get_variant_flag(uint32_t full_instruction);