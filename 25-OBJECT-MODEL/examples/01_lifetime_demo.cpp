// 01_lifetime_demo.cpp
// ============================================================
// Object lifetime: kab SHURU hota hai (ctor khatam), kab KHATAM (dtor
// shuru), aur storage lifetime se alag hai. Storage reuse (placement
// new) se ek hi jagah pe kai objects, ek ke baad ek.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 01_lifetime_demo.cpp -o lt && ./lt
// ============================================================

#include <cstdio>
#include <cstdint>
#include <new>          // placement new, std::launder
#include <string>
#include <utility>

// ---- ek type jo apni poori zindagi log karta ----
struct Tracer {
    int id;
    static inline int next_id = 0;
    static inline int alive   = 0;

    Tracer() : id(next_id++) { ++alive; std::printf("  [%d] ctor   (alive=%d)\n", id, alive); }
    explicit Tracer(const char* tag) : id(next_id++) {
        ++alive; std::printf("  [%d] ctor(%s) (alive=%d)\n", id, tag, alive);
    }
    ~Tracer() { --alive; std::printf("  [%d] dtor   (alive=%d)\n", id, alive); }

    Tracer(const Tracer& o) : id(next_id++) {
        ++alive; std::printf("  [%d] copy from [%d] (alive=%d)\n", id, o.id, alive);
    }
    Tracer& operator=(const Tracer&) { std::printf("  [%d] copy-assign\n", id); return *this; }

    void hello() const { std::printf("  [%d] hello, I am alive\n", id); }
};

int main() {
    // --------------------------------------------------------
    //  1. Automatic object — lifetime = scope
    // --------------------------------------------------------
    std::puts("1. automatic storage, lifetime = { } block:");
    {
        Tracer a;                 // lifetime SHURU yahan (ctor complete)
        a.hello();
    }                             // lifetime KHATAM yahan (dtor), storage bhi free
    std::puts("   (block chhoda -> a destruct ho gaya)\n");

    // --------------------------------------------------------
    //  2. Lifetime != storage duration.
    //     `char buf[]` ka STORAGE poore scope ka hai, par usme jo
    //     Tracer OBJECT hum banate hain uska lifetime hum control karte.
    // --------------------------------------------------------
    std::puts("2. manual lifetime in a raw buffer (placement new):");
    alignas(Tracer) unsigned char buf[sizeof(Tracer)];   // sirf STORAGE, koi object nahi

    Tracer* p = ::new (buf) Tracer("in-buf");   // ab yahan ek Tracer ka lifetime shuru
    p->hello();
    p->~Tracer();                                // lifetime khatam — par `buf` storage abhi zinda
    std::puts("   (object destruct — buffer storage abhi bhi allocated)\n");

    // --------------------------------------------------------
    //  3. Storage REUSE — ek hi buffer, teen alag-alag objects
    //     ek ke baad ek. Har naya placement-new ek naya object hai
    //     (naya lifetime, naya identity).
    // --------------------------------------------------------
    std::puts("3. same storage, 3 objects one after another:");
    for (int i = 0; i < 3; ++i) {
        Tracer* q = ::new (buf) Tracer;    // naya object, wahi bytes
        q->hello();
        q->~Tracer();
    }
    std::puts("");

    // --------------------------------------------------------
    //  4. Ek object ki jagah DOOSRE type ka object (union-like reuse).
    //     Pehle wale ka dtor call karna aapki zimmedari.
    // --------------------------------------------------------
    std::puts("4. reuse storage for a different type:");
    alignas(std::max_align_t) unsigned char slot[64];

    using Str = std::string;
    auto* s = ::new (slot) Str("hello lifetime");
    std::printf("  string in slot: \"%s\" (len %zu)\n", s->c_str(), s->size());
    s->~Str();                                // string destruct (pseudo-dtor via alias)

    auto* n = ::new (slot) std::int64_t{0x4142434445464748};
    std::printf("  int64 in same slot: 0x%llx\n", static_cast<unsigned long long>(*n));
    // trivial type -> dtor call optional, par consistency ke liye:
    // (std::int64_t ka dtor trivial hai)
    std::puts("");

    // --------------------------------------------------------
    //  5. Lifetime aur `const` / references — dangling ka beej
    //     (yahan sirf dikhaya, trigger nahi kiya)
    // --------------------------------------------------------
    std::puts("5. lifetime rules ka natija (detail file 02, 05 mein):");
    std::puts("   - object use karo SIRF ctor-complete se dtor-start ke beech");
    std::puts("   - us window ke bahar: UB (padho garbage, crash, ya 'kaam kar gaya')");
    std::puts("   - storage zinda hone se object zinda nahi hota");

    std::printf("\nfinal alive count = %d (0 hona chahiye)\n", Tracer::alive);
    return 0;
}
