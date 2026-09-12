// 03_vtable_layout.cpp
// ============================================================
// vptr aur vtable -- object mein hidden pointer, dispatch ka mechanism
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_vtable_layout.cpp -o vt && ./vt
//   Assembly bhi dekho: g++ -std=c++20 -O2 -S -masm=intel 03_vtable_layout.cpp
// ============================================================
//   Ek class jisme koi virtual function ho:
//   - Object ke SHURU mein ek hidden pointer: vptr  (usually 8 bytes)
//   - vptr -> class ki vtable (function pointers ka array, .rodata mein)
//   - obj->vfunc()  =>  (*(obj->vptr[index]))(obj)   -- ek indirect call
//   Sab objects of one class -> same vtable share karte (vtable per-class, per-object nahi)
// ============================================================

#include <cstdint>
#include <iostream>

struct NoVirtual {
    int a_;
    int b_;
    void f() {}
};

struct OneVirtual {
    int a_;
    int b_;
    virtual void f() { std::cout << "  OneVirtual::f\n"; }
    virtual ~OneVirtual() = default;
};

struct Base {
    virtual void speak() const { std::cout << "  Base::speak\n"; }
    virtual int  value() const { return 1; }
    virtual ~Base() = default;
};

struct Derived : Base {
    void speak() const override { std::cout << "  Derived::speak\n"; }
    int  value() const override { return 2; }
};

int main() {
    std::cout << "=== sizeof: virtual = +1 hidden pointer ===\n";
    std::cout << "  sizeof(NoVirtual)  = " << sizeof(NoVirtual)  << "   (2 ints = 8)\n";
    std::cout << "  sizeof(OneVirtual) = " << sizeof(OneVirtual) << "   (8-byte vptr + 2 ints + pad)\n";
    std::cout << "  sizeof(Base)       = " << sizeof(Base)       << "   (sirf vptr)\n";
    std::cout << "  sizeof(Derived)    = " << sizeof(Derived)    << "   (Base subobject -- same vptr slot)\n";

    std::cout << "\n=== vptr object ke SHURU mein hota hai ===\n";
    OneVirtual ov;
    ov.a_ = 111; ov.b_ = 222;
    // pehle 8 bytes = vptr; uske baad members
    auto* raw = reinterpret_cast<std::uint64_t*>(&ov);
    std::cout << "  &ov                       = " << &ov << "\n";
    std::cout << "  *(uint64*)&ov  (the vptr) = 0x" << std::hex << raw[0] << std::dec << "\n";
    std::cout << "  a_ @ offset " << (reinterpret_cast<char*>(&ov.a_) - reinterpret_cast<char*>(&ov))
              << ", b_ @ offset " << (reinterpret_cast<char*>(&ov.b_) - reinterpret_cast<char*>(&ov))
              << "   (members vptr ke BAAD)\n";

    std::cout << "\n=== same class -> same vtable (shared per-class) ===\n";
    OneVirtual ov2;
    auto* raw2 = reinterpret_cast<std::uint64_t*>(&ov2);
    std::cout << "  ov.vptr  = 0x" << std::hex << raw[0]  << "\n";
    std::cout << "  ov2.vptr = 0x" << std::hex << raw2[0] << std::dec
              << "   -> " << (raw[0] == raw2[0] ? "SAME vtable" : "different") << "\n";

    std::cout << "\n=== dispatch: Base* -> Derived object ===\n";
    Derived dObj;
    Base* bp = &dObj;
    auto* bpraw = reinterpret_cast<std::uint64_t*>(bp);
    std::cout << "  bp->vptr = 0x" << std::hex << bpraw[0] << std::dec
              << "   (Derived ki vtable -- construction ne set ki)\n";
    std::cout << "  bp->speak() -> ";  bp->speak();       // Derived::speak (via vtable)
    std::cout << "  bp->value() -> " << bp->value() << "\n";  // 2

    std::cout <<
        "\n"
        "  virtual class object = [ vptr | members... ]\n"
        "  vptr -> per-class vtable (function pointers) in .rodata\n"
        "  obj->vf() = load vptr -> load slot -> indirect call(obj)\n"
        "  Constructor har object ka vptr apni class ki vtable pe set karta hai.\n"
        "  Cost: ek extra load + ek indirect call (inline nahi ho sakta) -- file 12.\n";
    return 0;
}
