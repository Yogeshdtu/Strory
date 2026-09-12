// 03_virtual_call_asm.cpp
// ============================================================
// Virtual call vs direct call vs CRTP -- assembly mein farak dekho.
//
//   ./build.ps1 asm 34-ASSEMBLY/examples/03_virtual_call_asm.cpp
//
// Dekhne ki cheezein:
//   direct call    : `call _ZN6Direct4areaEv`         (name, resolved at compile)
//   virtual call   : `mov rax, [rdi]`  (load vptr)
//                    `call [rax+8]`     (call vtable slot -- INDIRECT)
//   CRTP           : NO call -- body inlined right there
//   devirtualized  : virtual through a KNOWN concrete type -> direct/inlined
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_virtual_call_asm.cpp -o vca && ./vca
// ============================================================

#include <cstdio>
#include <cstdint>
#include <memory>

// via_* functions non-static hain -> standalone asm emit hoti hai; printf sink.

// ---- virtual ----
struct Shape {
    virtual ~Shape() = default;
    virtual std::int64_t area() const = 0;
};
struct Rect final : Shape {
    std::int64_t w, h;
    Rect(std::int64_t a, std::int64_t b) : w(a), h(b) {}
    std::int64_t area() const override { return w * h; }
};

// ---- direct (non-virtual) ----
struct DirectRect {
    std::int64_t w, h;
    std::int64_t area() const { return w * h; }   // static call, inlinable
};

// ---- CRTP (static polymorphism) ----
template <class D>
struct ShapeBase {
    std::int64_t area() const { return static_cast<const D*>(this)->area_impl(); }
};
struct CrtpRect : ShapeBase<CrtpRect> {
    std::int64_t w, h;
    CrtpRect(std::int64_t a, std::int64_t b) : w(a), h(b) {}
    std::int64_t area_impl() const { return w * h; }
};

// each of these is a separate function -> read its asm
std::int64_t via_virtual(const Shape& s)     { return s.area(); }          // call [vtbl+slot]
std::int64_t via_direct(const DirectRect& r) { return r.area(); }          // inlined: imul
std::int64_t via_crtp(const CrtpRect& r)     { return r.area(); }          // inlined: imul
std::int64_t via_known_concrete(const Rect& r){ return r.area(); }         // devirt: Rect is final -> direct

int main() {
    std::unique_ptr<Shape> sp = std::make_unique<Rect>(6, 7);
    DirectRect dr{6, 7};
    CrtpRect   cr{6, 7};
    Rect       rc{6, 7};

    std::int64_t a = via_virtual(*sp);
    std::int64_t b = via_direct(dr);
    std::int64_t c = via_crtp(cr);
    std::int64_t d = via_known_concrete(rc);

    std::printf("via_virtual        = %lld\n", static_cast<long long>(a));
    std::printf("via_direct         = %lld\n", static_cast<long long>(b));
    std::printf("via_crtp           = %lld\n", static_cast<long long>(c));
    std::printf("via_known_concrete = %lld\n", static_cast<long long>(d));

    std::puts("\nRead the asm of each function:");
    std::puts("  ./build.ps1 asm 34-ASSEMBLY/examples/03_virtual_call_asm.cpp");
    std::puts("  via_virtual        : mov rax,[rdi] ; call [rax+..]   <- INDIRECT (vtable)");
    std::puts("  via_direct         : mov rax,[rdi] ; imul rax,[rdi+8] <- inlined, no call");
    std::puts("  via_crtp           : same as direct -- inlined imul");
    std::puts("  via_known_concrete : Rect is `final` + concrete type known -> devirtualized");
    std::puts("                       to a direct call, then inlined -> imul, no vtable");
    std::printf("\nsizeof  Rect(vptr)=%zu  DirectRect=%zu  CrtpRect=%zu  (vptr adds 8)\n",
               sizeof(Rect), sizeof(DirectRect), sizeof(CrtpRect));
    return 0;
}
