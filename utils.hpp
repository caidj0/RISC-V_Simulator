template <typename Fn>
class Wire {
    const Fn f;

   public:
    Wire(Fn f) : f(f) {}
    auto value() -> decltype(f()) const { return f(); }
    operator decltype(f())() const { return f(); }
};

#define WIRE(expr) Wire([&]() {return (expr);})