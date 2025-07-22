#pragma once

#include <functional>

template <typename T>
class Wire {
    const std::function<T(void)> f;

   public:
    Wire() : f([]() { return T(); }) {}
    Wire(std::function<T(void)> f) : f(f) {}
    operator T() const { return f(); }
};

#define WIRE(expr) Wire<decltype(expr)>([&]() { return (expr); })

class Updatable {
    virtual void pull() = 0;
    virtual void update() = 0;
};

template <typename T>
class Reg : Updatable {
    const std::function<T(void)> f;

    T value;
    T new_value;

   public:
    Reg() : f([]() { return T(); }) {}
    Reg(std::function<T(void)> f) : f(f) {}
    operator T() const { return value; }
    void pull() { new_value = f(); }
    void update() { value = new_value; }
};

#define REG(expr) Reg<decltype(expr)>([&]() { return (expr); })