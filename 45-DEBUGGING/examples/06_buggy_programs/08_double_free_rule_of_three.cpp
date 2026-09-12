// 08_double_free_rule_of_three.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 08_double_free_rule_of_three.cpp -o t && ./t
//   g++ -std=c++20 -Wall -Wextra -O2 08_double_free_rule_of_three.cpp -o t   # <- GCC yahan warn karta
// Expected: "buf[0]=7 ... done"   Actual: "buf[0]=7 ..." phir crash (double free).
// ============================================================
#include <cstdio>
#include <cstring>

// ek chhota owning buffer -- raw new/delete (jaan-boojh kar, seekhne ke liye)
class Buffer {
public:
    explicit Buffer(std::size_t n) : n_(n), data_(new int[n]) {
        std::memset(data_, 0, n_ * sizeof(int));
    }
    ~Buffer() { delete[] data_; }             // <-- ek dtor, do frees ban jaayenge

    std::size_t size() const { return n_; }
    int&  operator[](std::size_t i)       { return data_[i]; }
    int   operator[](std::size_t i) const { return data_[i]; }
    // BUG: copy constructor / copy assignment define NAHI kiye,
    //      aur delete bhi nahi kiye.
private:
    std::size_t n_;
    int*        data_;
};

static void use(Buffer copy) {                // <-- by value -> compiler-generated copy
    std::printf("buf[0]=%d (size %zu)\n", copy[0], copy.size());
}                                             // <-- yahan `copy` ka dtor: delete[] data_

int main() {
    Buffer original(4);
    original[0] = 7;
    use(original);                            // copy banti hai; dono `data_` same pointer
    std::printf("done\n");                    // <-- shayad na chhape: original ka dtor
    return 0;                                 //     usi pointer ko dobara delete[] karega
}

// ============================================================
// BUG:     `Buffer` ek raw `int*` OWN karta (`~Buffer` `delete[]` karta),
//          par copy constructor define nahi kiya. Compiler-generated copy
//          MEMBERWISE hai -> `data_` pointer bit-copy ho jaata. Ab do
//          `Buffer` objects SAME pointer ko own karte. Dono ke dtor
//          chalne pe `delete[]` DO baar same block pe -> double free (UB).
//          "Rule of Three" ka violation (17-RAII, 18-COPY-MOVE).
// SYMPTOM: `use(original)` ka `copy` scope-end pe delete[] karta; phir
//          `main` ka `original` bhi. Crash: "free(): double free detected"
//          / "corrupted" / segfault -- aksar `main` ke return pe.
// TOOL:    ASan -> "attempting double-free" + dono free ki stack traces +
//          allocation site. valgrind memcheck -> "Invalid free() ... block
//          was already freed". GCC `-O2` pe `-Wuse-after-free` khud yeh
//          inlined double-`delete[]` dekh leta (`-O0` pe analysis off).
//          `-Weffc++` (noisy) rule-of-three miss pe warn karta. gdb:
//          `break ~Buffer` -> dono baar `print data_` -> same address.
// FIX:     Rule of Zero -- raw pointer HATAO:
//            std::vector<int> data_;            // ya std::unique_ptr<int[]>
//          ab koi manual dtor/copy/move ki zarurat nahi, double-free
//          structurally impossible. Agar raw rakhna hi hai: deep-copy
//          copy-ctor + copy-assign (copy-and-swap), ya `= delete` unhe.
// ============================================================
