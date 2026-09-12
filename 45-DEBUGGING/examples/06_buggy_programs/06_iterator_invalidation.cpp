// 06_iterator_invalidation.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 06_iterator_invalidation.cpp -o t && ./t
// Expected: "kept 3 of 6"   Actual: crash / kept wrong count / skipped elems.
// ============================================================
#include <cstdio>
#include <vector>

// even numbers ko list se hata do (in-place)
static void drop_evens(std::vector<int>& v) {
    for (auto it = v.begin(); it != v.end(); ++it) {   // <-- dekho yahan
        if (*it % 2 == 0) {
            v.erase(it);
        }
    }
}

int main() {
    std::vector<int> v{1, 2, 4, 3, 6, 5};   // odds: 1,3,5 -> "kept 3 of 6"
    const std::size_t before = v.size();
    drop_evens(v);
    std::printf("kept %zu of %zu:", v.size(), before);
    for (int x : v) std::printf(" %d", x);
    std::printf("\n");
    return 0;
}

// ============================================================
// BUG:     `v.erase(it)` ke baad `it` INVALIDATED hai. `std::vector::erase`
//          erased element ke baad wale saare iterators (aur `it` khud) ko
//          invalid kar deta -- fir `++it` UB, aur `*it`/`v.end()` compare
//          bhi. Consecutive evens (4,6) ke case mein element skip bhi hota.
// SYMPTOM: Kabhi crash, kabhi galat count, kabhi ek even bach jaata (jaise
//          "2,4" mein se sirf 2 hata). `-O2` pe alag behaviour.
// TOOL:    `-D_GLIBCXX_DEBUG` (libstdc++ debug mode) -> runtime pe
//          "attempt to dereference a past-the-end / invalidated iterator"
//          exact line. ASan -> heap-buffer-overflow (erase ke baad
//          shifted buffer). gdb `watch` on `it._M_current`.
// FIX:     `erase` ka RETURN VALUE use karo (agla valid iterator):
//            for (auto it = v.begin(); it != v.end(); ) {
//                if (*it % 2 == 0) it = v.erase(it);
//                else ++it;
//            }
//          Ya idiomatic (C++20): `std::erase_if(v, [](int x){ return x%2==0; });`
//          (erase-remove se bhi: `v.erase(std::remove_if(...), v.end());`).
//          (19-STL/iterators, 20-DSA, 09-ARRAYS/11).
// ============================================================
