// 04_move_semantics.cpp
// ============================================================
// Move constructor & move assignment -- resource stealing, O(1) transfer
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_move_semantics.cpp -o mv && ./mv
// ============================================================
//   COPY : naya buffer allocate + poora data memcpy  -> O(n)
//   MOVE : source ke pointer/size STEAL karo, source ko empty/valid state pe chhodo -> O(1)
//   Move ctor/assign RVALUES (temporaries, std::move(x)) ko consume karti hain.
// ============================================================

#include <cstring>
#include <iostream>
#include <utility>

class Buffer {
    std::size_t n_    = 0;
    int*        data_ = nullptr;

public:
    Buffer() = default;

    explicit Buffer(std::size_t n) : n_(n), data_(new int[n]) {
        std::cout << "  Buffer(" << n_ << ")  ctor        alloc @ " << static_cast<void*>(data_) << "\n";
    }

    // ---- COPY ctor -- deep, O(n) ----
    Buffer(const Buffer& o) : n_(o.n_), data_(new int[o.n_]) {
        std::memcpy(data_, o.data_, n_ * sizeof(int));
        std::cout << "  Buffer(const Buffer&)  COPY  (" << n_ << " ints memcpy'd)  new @ "
                  << static_cast<void*>(data_) << "\n";
    }

    // ---- MOVE ctor -- steal, O(1), noexcept (file 13) ----
    Buffer(Buffer&& o) noexcept : n_(o.n_), data_(o.data_) {
        o.n_    = 0;
        o.data_ = nullptr;                                    // source -> valid empty state
        std::cout << "  Buffer(Buffer&&)  MOVE  (stole ptr @ " << static_cast<void*>(data_)
                  << ", source now empty)\n";
    }

    // ---- MOVE assignment -- release old, steal, null source ----
    Buffer& operator=(Buffer&& o) noexcept {
        std::cout << "  operator=(Buffer&&)  MOVE assign\n";
        if (this != &o) {
            delete[] data_;
            n_      = o.n_;
            data_   = o.data_;
            o.n_    = 0;
            o.data_ = nullptr;
        }
        return *this;
    }

    // ---- COPY assignment (for contrast) ----
    Buffer& operator=(const Buffer& o) {
        std::cout << "  operator=(const Buffer&)  COPY assign  (" << o.n_ << " ints)\n";
        if (this != &o) {
            int* fresh = new int[o.n_];
            std::memcpy(fresh, o.data_, o.n_ * sizeof(int));
            delete[] data_;
            data_ = fresh; n_ = o.n_;
        }
        return *this;
    }

    ~Buffer() {
        std::cout << "  ~Buffer(" << n_ << ")  free @ " << static_cast<void*>(data_) << "\n";
        delete[] data_;
    }

    std::size_t size() const { return n_; }
};

Buffer makeBuffer(std::size_t n) { return Buffer{n}; }        // returns a prvalue (elided / moved)

int main() {
    std::cout << "=== 1. copy vs move construction ===\n";
    Buffer a{4};
    Buffer b = a;                       // COPY ctor -- new buffer, memcpy
    Buffer c = std::move(a);            // MOVE ctor -- steal a's pointer; a now empty
    std::cout << "  a.size()=" << a.size() << " (moved-from)   b.size()=" << b.size()
              << "   c.size()=" << c.size() << "\n";

    std::cout << "\n=== 2. move assignment ===\n";
    Buffer d{2};
    d = std::move(b);                   // MOVE assign -- d frees its old, steals b's; b empty
    std::cout << "  b.size()=" << b.size() << " (moved-from)   d.size()=" << d.size() << "\n";

    std::cout << "\n=== 3. temporary -> move (rvalue) ===\n";
    Buffer e = makeBuffer(6);           // prvalue -> guaranteed elision (C++17) OR move ctor
    std::cout << "  e.size()=" << e.size() << "\n";
    e = makeBuffer(8);                  // rvalue -> MOVE assignment (temporary consumed)
    std::cout << "  e.size()=" << e.size() << "\n";

    std::cout << "\n=== 4. moved-from is VALID (just empty) -- reuse OK ===\n";
    a = Buffer{3};                      // assign a fresh value into the moved-from 'a'
    std::cout << "  a.size()=" << a.size() << "   (moved-from object reassigned -- fine)\n";

    std::cout << "\n=== destructors ===\n";
    return 0;
}
