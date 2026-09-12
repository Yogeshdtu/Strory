// 05_placement_new.cpp
// ============================================================
// Placement new: storage aur object lifetime ko ALAG-ALAG control karo.
// - aligned raw storage
// - manual construct (placement new) + manual destruct (explicit ~T())
// - std::launder ka role
// - ek chhota FixedOptional<T> jo isi pe bana hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 05_placement_new.cpp -o pn && ./pn
// ============================================================

#include <cstdio>
#include <cstddef>
#include <new>          // placement new, std::launder
#include <string>
#include <utility>
#include <type_traits>

struct Widget {
    std::string tag;
    int n;
    static inline int alive = 0;
    Widget(std::string t, int k) : tag(std::move(t)), n(k) {
        ++alive; std::printf("  Widget(\"%s\", %d) ctor  (alive=%d)\n", tag.c_str(), n, alive);
    }
    ~Widget() { --alive; std::printf("  ~Widget(\"%s\")  (alive=%d)\n", tag.c_str(), alive); }
    void use() const { std::printf("  use: %s x %d\n", tag.c_str(), n); }
};

// ============================================================
//  FixedOptional<T> — heap ke bina "hai ya nahi". std::optional ka
//  bare-bones version, placement new + explicit dtor pe bana.
// ============================================================
template <class T>
class FixedOptional {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool engaged_ = false;

    // cast via void* -> no -Wcast-align; storage_ is alignas(T) anyway
    T* ptr() { return std::launder(static_cast<T*>(static_cast<void*>(storage_))); }
    const T* ptr() const {
        return std::launder(static_cast<const T*>(static_cast<const void*>(storage_)));
    }

public:
    FixedOptional() = default;
    ~FixedOptional() { reset(); }

    FixedOptional(const FixedOptional&) = delete;
    FixedOptional& operator=(const FixedOptional&) = delete;

    template <class... Args>
    T& emplace(Args&&... args) {
        reset();
        ::new (static_cast<void*>(storage_)) T(std::forward<Args>(args)...);  // construct in place
        engaged_ = true;
        return *ptr();
    }

    void reset() {
        if (engaged_) {
            ptr()->~T();                // explicit destructor call
            engaged_ = false;
        }
    }

    bool has_value() const { return engaged_; }
    T&       operator*()       { return *ptr(); }
    const T& operator*() const { return *ptr(); }
    T*       operator->()      { return ptr(); }
};

int main() {
    // --------------------------------------------------------
    //  1. Raw aligned storage + manual lifecycle
    // --------------------------------------------------------
    std::puts("1. manual construct / destruct in aligned storage:");
    alignas(Widget) unsigned char buf[sizeof(Widget)];

    Widget* w = ::new (static_cast<void*>(buf)) Widget("alpha", 3);   // construct
    w->use();
    w->~Widget();                                                     // destruct
    std::puts("   (storage `buf` abhi bhi allocated — sirf object gaya)\n");

    // --------------------------------------------------------
    //  2. Reuse the SAME storage for a new object
    // --------------------------------------------------------
    std::puts("2. reuse storage for a second Widget:");
    Widget* w2 = ::new (static_cast<void*>(buf)) Widget("beta", 7);
    w2->use();
    w2->~Widget();
    std::puts("");

    // --------------------------------------------------------
    //  3. std::launder — kab chahiye
    // --------------------------------------------------------
    std::puts("3. std::launder note:");
    std::puts("   Jab tum placement-new se ek NAYA object banate ho usi memory pe");
    std::puts("   jahan purana tha, to purane object ka pointer/reference se naye ko");
    std::puts("   access karna UB ho sakta (compiler purani value cache kar sakta,");
    std::puts("   khaas kar const/reference members ke saath). std::launder(p) compiler");
    std::puts("   ko batata: 'is address pe jo ab hai usse dekho, jo pehle tha usse nahi.'");
    std::puts("   FixedOptional<T> upar har access pe std::launder use karta hai.\n");

    // --------------------------------------------------------
    //  4. FixedOptional<T> — placement new ka real use
    // --------------------------------------------------------
    std::puts("4. FixedOptional<Widget> (no heap):");
    {
        FixedOptional<Widget> opt;
        std::printf("   has_value = %d\n", opt.has_value());
        opt.emplace("gamma", 11);
        std::printf("   has_value = %d\n", opt.has_value());
        opt->use();
        opt.emplace("delta", 99);          // purana reset (dtor) + naya construct
        (*opt).use();
    }                                      // ~FixedOptional -> reset -> ~Widget
    std::puts("   (FixedOptional scope-end pe apna Widget destruct karta hai)\n");

    std::printf("Widget::alive at end = %d (0 hona chahiye)\n", Widget::alive);

    // --------------------------------------------------------
    //  5. sizeof check — koi hidden allocation nahi
    // --------------------------------------------------------
    std::printf("sizeof(FixedOptional<Widget>) = %zu vs sizeof(Widget) = %zu (+ bool + pad)\n",
                sizeof(FixedOptional<Widget>), sizeof(Widget));
    static_assert(std::is_trivially_destructible_v<int>);
    static_assert(sizeof(FixedOptional<int>) >= sizeof(int) + 1);
    return 0;
}
