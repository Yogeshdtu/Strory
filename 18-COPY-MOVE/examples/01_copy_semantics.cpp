// 01_copy_semantics.cpp
// ============================================================
// Copy constructor & copy assignment -- kab chalte hain, deep vs shallow
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_copy_semantics.cpp -o cs && ./cs
// ============================================================
//   Copy constructor  : T b = a;   T b(a);   f(a) [by value]   return a;  (pre-elision)
//   Copy assignment   : b = a;   (b already exists)
//   Deep copy   : apna alag resource -> independent
//   Shallow copy: same underlying pointer -> aliasing -> double-free (Rule of 3 -- file 03)
// ============================================================

#include <cstring>
#include <iostream>

// Ek chhoti class jo apna heap buffer OWN karti hai -- deep copy karti hai
class Str {
    std::size_t len_  = 0;              // declared FIRST -> init FIRST (order matters, file 03 / folder 15)
    char*       data_ = nullptr;

public:
    Str() = default;

    Str(const char* s)                                   // from C-string
        : len_(std::strlen(s)), data_(new char[len_ + 1]) {
        std::memcpy(data_, s, len_ + 1);
        std::cout << "  Str(\"" << s << "\")  ctor\n";
    }

    // ---- COPY CONSTRUCTOR -- deep copy ----
    Str(const Str& other)
        : len_(other.len_), data_(new char[other.len_ + 1]) {
        std::memcpy(data_, other.data_, len_ + 1);
        std::cout << "  Str(const Str&)  COPY ctor  [\"" << other.data_ << "\"]  new buffer @ "
                  << static_cast<const void*>(data_) << "\n";
    }

    // ---- COPY ASSIGNMENT -- deep copy, self-assignment safe ----
    Str& operator=(const Str& other) {
        std::cout << "  operator=(const Str&)  COPY assign  [\"" << other.data_ << "\"]\n";
        if (this != &other) {                            // self-assignment guard
            char* fresh = new char[other.len_ + 1];      // allocate FIRST (strong-ish)
            std::memcpy(fresh, other.data_, other.len_ + 1);
            delete[] data_;                              // then release old
            data_ = fresh;
            len_  = other.len_;
        }
        return *this;
    }

    ~Str() {
        std::cout << "  ~Str()  [\"" << (data_ ? data_ : "") << "\"]  free @ "
                  << static_cast<const void*>(data_) << "\n";
        delete[] data_;
    }

    const char* c_str() const { return data_ ? data_ : ""; }
};

void takeByValue(Str s) {                                // by value -> COPY on call
    std::cout << "    inside takeByValue: " << s.c_str() << "\n";
}

int main() {
    std::cout << "=== 1. copy CONSTRUCTION ===\n";
    Str a{"hello"};
    Str b = a;                     // copy ctor (NOT assignment -- b is new)
    Str c{a};                      // copy ctor
    std::cout << "  a=" << a.c_str() << "  b=" << b.c_str() << "  c=" << c.c_str() << "\n";

    std::cout << "\n=== 2. copy ASSIGNMENT ===\n";
    Str d{"x"};
    d = a;                         // copy assignment (d already exists)
    std::cout << "  d=" << d.c_str() << "\n";

    std::cout << "\n=== 3. self-assignment (guarded) ===\n";
    d = d;                         // this == &other -> no-op body
    std::cout << "  d still = " << d.c_str() << "\n";

    std::cout << "\n=== 4. pass by value -> copy ===\n";
    takeByValue(a);               // copy ctor for the parameter

    std::cout << "\n=== 5. independence (deep copy) ===\n";
    Str e{"one"};
    Str f = e;
    f = Str{"two"};               // reassign f; e unaffected
    std::cout << "  e=" << e.c_str() << "  f=" << f.c_str() << "   (independent -- deep copy)\n";

    std::cout << "\n=== destructors (reverse order) ===\n";
    return 0;
}
