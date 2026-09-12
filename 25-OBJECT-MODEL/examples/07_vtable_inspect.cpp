// 07_vtable_inspect.cpp
// ============================================================
// Ek polymorphic object ke andar jhaank ke dekho: pehle 8 bytes = vptr
// (Itanium ABI, GCC/Clang, x86-64). Same dynamic type -> same vtable.
// Multiple inheritance -> DO vptr. vtable ke slots function pointers.
//
// ⚠️ Yeh sab IMPLEMENTATION-DEFINED / UB-adjacent hai — sirf seekhne ke
//    liye, production code mein kabhi nahi. Portable nahi.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 07_vtable_inspect.cpp -o vt && ./vt
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstring>

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual const char* name() const = 0;
};

struct Circle : Shape {
    double r;
    explicit Circle(double rr) : r(rr) {}
    double area() const override { return 3.141592653589793 * r * r; }
    const char* name() const override { return "Circle"; }
};

struct Square : Shape {
    double s;
    explicit Square(double ss) : s(ss) {}
    double area() const override { return s * s; }
    const char* name() const override { return "Square"; }
};

// multiple inheritance -> two vptr
struct Printable { virtual ~Printable() = default; virtual void print() const = 0; };
struct Both : Shape, Printable {
    double area() const override { return 1.0; }
    const char* name() const override { return "Both"; }
    void print() const override { std::printf("  Both::print()\n"); }
};

static const void* vptr_of(const void* obj) {
    const void* v;
    std::memcpy(&v, obj, sizeof v);      // pehle pointer-size bytes = vptr (Itanium)
    return v;
}

int main() {
    Circle c1{2.0}, c2{5.0};
    Square s1{3.0};

    std::printf("sizeof(Circle) = %zu  (double r + vptr)\n", sizeof(Circle));
    std::printf("sizeof: Shape*=%zu\n\n", sizeof(Shape*));

    // --------------------------------------------------------
    //  1. vptr — same type share karte hain
    // --------------------------------------------------------
    std::puts("1. vptr (first 8 bytes of the object):");
    std::printf("  &c1        = %p   vptr = %p\n", static_cast<void*>(&c1), vptr_of(&c1));
    std::printf("  &c2        = %p   vptr = %p\n", static_cast<void*>(&c2), vptr_of(&c2));
    std::printf("  &s1        = %p   vptr = %p\n", static_cast<void*>(&s1), vptr_of(&s1));
    std::printf("  c1 & c2 same vtable? %s\n", vptr_of(&c1) == vptr_of(&c2) ? "yes" : "no");
    std::printf("  c1 & s1 same vtable? %s\n\n", vptr_of(&c1) == vptr_of(&s1) ? "yes" : "no");

    // --------------------------------------------------------
    //  2. vtable = per-class table of function pointers
    //     Itanium: object[0] = vptr -> vtable. vtable ke aas-paas
    //     slots: [-2] offset-to-top, [-1] RTTI (&type_info), [0..] the
    //     virtual functions (dtor variants, then declared order — exact
    //     layout impl-defined, isliye hum slots ko manually call nahi karte).
    //     Virtual call ka normal path yahi hai: load vptr, load slot, call.
    // --------------------------------------------------------
    std::puts("2. dispatch goes through the vtable:");
    {
        const Shape* sp = &c1;                         // static type Shape*
        std::printf("  sp->area()  = %.4f  (%s)   <- resolved via c1's vtable\n",
                    sp->area(), sp->name());
        std::printf("  vptr(&c1) = %p  (points at Circle's vtable in .rodata)\n",
                    vptr_of(&c1));
    }
    std::puts("");

    // --------------------------------------------------------
    //  3. Multiple inheritance — do vptr, alag base subobjects
    // --------------------------------------------------------
    std::puts("3. multiple inheritance (Both : Shape, Printable):");
    Both b;
    Shape*     as_shape = &b;
    Printable* as_print = &b;
    std::printf("  &b                    = %p\n", static_cast<void*>(&b));
    std::printf("  (Shape*)&b            = %p   vptr = %p\n",
                static_cast<void*>(as_shape), vptr_of(as_shape));
    std::printf("  (Printable*)&b        = %p   vptr = %p\n",
                static_cast<void*>(as_print), vptr_of(as_print));
    std::printf("  offset(Printable in Both) = %ld bytes\n",
                static_cast<long>(reinterpret_cast<const char*>(as_print) -
                                  reinterpret_cast<const char*>(&b)));
    std::puts("  ^ Printable subobject apne alag vptr ke saath, non-zero offset pe.");
    std::puts("    Isliye `static_cast<Printable*>(&b)` pointer ki VALUE badalta hai.");
    as_print->print();
    std::printf("  as_shape->area() = %.1f\n", as_shape->area());

    std::puts("\nSaar:");
    std::puts(" - polymorphic object ka pehla word = vptr (Itanium ABI)");
    std::puts(" - vptr -> per-CLASS vtable (saare instances share karte)");
    std::puts(" - vtable ke slots = virtual functions ke pointers (+ RTTI, offset-to-top)");
    std::puts(" - MI: har polymorphic base ka apna vptr -> base-cast pointer value shift karta");
    std::puts(" - yeh sab ABI detail hai — code mein depend mat karo (folder 24 file 14)");
    return 0;
}
