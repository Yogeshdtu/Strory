// 04_type_properties.cpp
// ============================================================
// Trivial / trivially-copyable / standard-layout / aggregate / POD.
// Kaunsa kya matlab, aur kaunse operations (memcpy, memcmp, malloc-
// then-use, reinterpret to bytes) kis property pe LEGAL hote hain.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 04_type_properties.cpp -o tp && ./tp
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <string>

// ---- helper: ek type ki saari properties ek line mein ----
template <class T>
static void report(const char* name) {
    std::printf("  %-22s  size=%2zu align=%2zu | trivial=%d triv-copy=%d "
                "std-layout=%d aggregate=%d uniq-obj-rep=%d\n",
                name, sizeof(T), alignof(T),
                std::is_trivial_v<T>,
                std::is_trivially_copyable_v<T>,
                std::is_standard_layout_v<T>,
                std::is_aggregate_v<T>,
                std::has_unique_object_representations_v<T>);
}

// ============================================================
//  Test types
// ============================================================

// A: plain — trivial, trivially-copyable, standard-layout, POD-jaisa
struct A { int x; int y; };

// B: has a user ctor -> NOT trivial (ctor non-trivial), NOT aggregate,
//    par abhi bhi trivially-copyable (copy/move/dtor trivial) aur std-layout
struct B {
    int x, y;
    B() : x(0), y(0) {}
};

// C: padding hole -> NOT unique-object-representation (padding bytes indeterminate)
struct C { char c; int i; };   // 1 byte + 3 pad + 4 int

// D: mixed access -> NOT standard-layout (public + private data members)
struct D {
    int pub;
private:
    int priv;
public:
    explicit D(int a, int b) : pub(a), priv(b) {}
    int sum() const { return pub + priv; }
};

// E: has a non-trivial member (std::string) -> NOT trivial, NOT triv-copyable
struct E {
    int id;
    std::string name;
};

// F: virtual function -> vptr -> NOT trivial, NOT std-layout, NOT triv-copyable
struct F {
    virtual ~F() = default;
    int v = 0;
};

// G: base with data + derived with data -> NOT standard-layout
struct GBase { int a; };
struct G : GBase { int b; };

int main() {
    std::puts("type properties (1 = yes):\n");
    report<int>("int");
    report<double>("double");
    report<A>("A{int,int}");
    report<B>("B (user ctor)");
    report<C>("C{char,int} pad");
    report<D>("D (mixed access)");
    report<E>("E{int,std::string}");
    report<F>("F (virtual dtor)");
    report<G>("G : GBase (both data)");

    // --------------------------------------------------------
    //  Kaunsa operation kaunsi property maangta
    // --------------------------------------------------------
    std::puts("\nwhat you may legally do:\n");

    // 1. memcpy between objects -> needs is_trivially_copyable
    std::puts("1. std::memcpy(&dst, &src, sizeof) — needs trivially_copyable:");
    {
        A a1{7, 8};
        A a2;
        std::memcpy(&a2, &a1, sizeof(A));           // ✅ A is trivially copyable
        std::printf("   A copied via memcpy: {%d, %d}\n", a2.x, a2.y);
        // E e1{1,"x"}; std::memcpy(&e2, &e1, sizeof(E));  // ❌ UB — std::string inside
        std::puts("   (E has std::string -> memcpy of E = UB: would copy the pointer, double-free)");
    }

    // 2. reinterpret an object as raw bytes and back -> trivially_copyable
    std::puts("\n2. object <-> byte array round-trip — needs trivially_copyable:");
    {
        A a{0x11223344, 0x55667788};
        unsigned char bytes[sizeof(A)];
        std::memcpy(bytes, &a, sizeof(A));          // ✅
        A back;
        std::memcpy(&back, bytes, sizeof(A));       // ✅
        std::printf("   round-trip: {%#x, %#x}\n", back.x, back.y);
    }

    // 3. memcmp for equality -> needs has_unique_object_representations
    //    (warna padding bytes garbage -> jhoota "not equal")
    std::puts("\n3. std::memcmp for equality — needs unique_object_representations:");
    {
        C c1; C c2;
        std::memset(&c1, 0, sizeof(C)); std::memset(&c2, 0, sizeof(C));
        c1.c = 'A'; c1.i = 42;
        c2.c = 'A'; c2.i = 42;
        // dono ko memset se poora zero kiya, phir same values -> memcmp 0
        std::printf("   after memset+assign, memcmp == %d (0 = equal)\n",
                    std::memcmp(&c1, &c2, sizeof(C)));
        // Bina memset: padding bytes indeterminate -> memcmp non-zero possible.
        C d1; C d2;
        d1.c = 'A'; d1.i = 42;
        d2.c = 'A'; d2.i = 42;
        std::printf("   WITHOUT memset first: memcmp == %d "
                    "(may be non-zero — padding bytes!)\n",
                    std::memcmp(&d1, &d2, sizeof(C)));
    }

    // 4. offsetof / C-compatible layout -> needs standard_layout
    std::puts("\n4. offsetof / share layout with C — needs standard_layout:");
    std::printf("   offsetof(A, y) = %zu\n", offsetof(A, y));
    std::printf("   offsetof(C, i) = %zu  (after char + padding)\n", offsetof(C, i));
    // offsetof(D, priv) -> not standard-layout -> conditionally-supported / UB-ish

    std::puts("\nSaar:");
    std::puts(" - trivially_copyable  -> memcpy the bytes safely (serialization, ring buffers)");
    std::puts(" - standard_layout     -> offsetof, C interop, first-member address == object address");
    std::puts(" - unique_obj_repr     -> memcmp/hash the bytes (no padding surprises)");
    std::puts(" - trivial             -> also default-constructible without running code");
    std::puts(" - aggregate           -> brace-init members directly, no user ctor");
    return 0;
}
