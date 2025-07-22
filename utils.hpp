#pragma once

#include <functional>

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
};

class Updatable {
    virtual void pull() = 0;
    virtual void update() = 0;
};

template <typename T>
class Reg : Updatable {
    T value;
    T new_value;

   public:
    std::function<T(void)> f;
    Reg(std::function<T(void)> f) : f(f), value(), new_value() {}
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