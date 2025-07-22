#pragma once

#include <functional>

template <typename T>
class Wire {
   public:
    std::function<T(void)> f;
    Wire() : f([]() { return T(); }) {}
    Wire(std::function<T(void)> f) : f(f) {}
    operator T() const { return f(); }
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
    Reg() : f([]() { return T(); }) {}
    Reg(std::function<T(void)> f) : f(f) {}
    operator T() const { return value; }
    void pull() { new_value = f(); }
    void update() { value = new_value; }
};

#define LAM(expr) [&]() { return (expr); }