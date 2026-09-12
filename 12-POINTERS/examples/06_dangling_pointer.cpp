// 06_dangling_pointer.cpp
// ============================================================
//  ⚠️  IS FILE MEIN JAAN-BOOJH KAR DANGLING-POINTER BUGS HAIN  ⚠️
// ============================================================
//  COMPILE theek hota hai. Chalane pe: kabhi "kaam kar jaata hai"
//  (purani value), kabhi garbage, kabhi crash -- yeh UNDEFINED BEHAVIOUR hai.
//
//  PAKADNE KA TAREEKA:
//   - AddressSanitizer (best):  Linux/macOS/Clang:
//       g++ -std=c++20 -fsanitize=address,undefined -g 06_dangling_pointer.cpp -o dp && ./dp
//     -> "heap-use-after-free" / "stack-use-after-return" exact line ke saath.
//   ⚠️ MinGW-w64 (yeh Windows toolchain) mein libasan NAHI -> WSL/Linux use karo.
//   - -Wdangling-pointer / -Wreturn-local-addr -- kuch cases compile-time pe.
//
//  ⚠️ `make folder` / `checkall` sirf COMPILE karte hain -> "OK" dikhega.
// ============================================================

#include <iostream>
#include <vector>

// ---- BUG 1: pointer to a local -- stack-use-after-return ----
int* danglingLocal() {
    int x = 42;
    return &x;                    // ⚠️ x is gone when the function returns
}

// ---- BUG 3: pointer into a vector, then vector reallocates ----
void vectorReallocDemo() {
    std::vector<int> v = {1, 2, 3};
    int* p = &v[0];              // p -> v's current buffer
    std::cout << "  before: *p = " << *p << "\n";
    for (int i = 0; i < 1000; ++i) v.push_back(i);   // ⚠️ reallocation -> p dangling
    std::cout << "  after 1000 push_backs: *p = " << *p
              << "   (⚠️ p now points into freed memory)\n";
}

int main() {
    // ============================================================
    //  BUG 1: use-after-return
    // ============================================================
    std::cout << "===== BUG 1: pointer to a local =====\n";
    int* p1 = danglingLocal();
    std::cout << "  *p1 = " << *p1 << "   (⚠️ x's frame is gone -- value is luck)\n";
    danglingLocal();                          // call again -- may overwrite that stack slot
    std::cout << "  *p1 = " << *p1 << "   (⚠️ may have changed -- pure UB)\n";

    // ============================================================
    //  BUG 2: use-after-free (heap)
    // ============================================================
    std::cout << "\n===== BUG 2: use-after-free =====\n";
    int* p2 = new int(99);
    std::cout << "  *p2 = " << *p2 << "\n";
    delete p2;                                // memory returned to allocator
    std::cout << "  after delete: *p2 = " << *p2 << "   (⚠️ freed -- garbage/old value)\n";
    *p2 = 7;                                  // ⚠️ writing to freed memory -- corrupts allocator state
    std::cout << "  wrote 7 to freed memory -- ASan would abort here\n";
    // delete p2;                             // ⚠️ double-free -- also UB

    // ============================================================
    //  BUG 3: dangling after container reallocation
    // ============================================================
    std::cout << "\n===== BUG 3: vector reallocation =====\n";
    vectorReallocDemo();

    // ============================================================
    //  BUG 4: dangling after scope ends
    // ============================================================
    std::cout << "\n===== BUG 4: pointer outlives its target's scope =====\n";
    int* p4 = nullptr;
    {
        int temp = 123;
        p4 = &temp;
    }                                        // temp destroyed here
    std::cout << "  *p4 = " << *p4 << "   (⚠️ temp's scope ended -- dangling)\n";

    // ============================================================
    //  FIXES
    // ============================================================
    std::cout << "\n-----------------------------------------------\n";
    std::cout <<
        "FIXES:\n"
        "  * Don't return &local -- return by value (RVO, folder 08), or take an out-param.\n"
        "  * After delete p; set p = nullptr; (turns UAF into a clean null-deref crash).\n"
        "  * Better: don't use raw new/delete -- std::vector / std::unique_ptr (folder 14, 17).\n"
        "  * vector: reserve() upfront, or re-fetch &v[i] after any size change.\n"
        "  * A pointer must not outlive what it points at. Match lifetimes.\n"
        "  * CI: -fsanitize=address,undefined (Linux) ; debug: -D_GLIBCXX_ASSERTIONS.\n";

    return 0;
}

// ============================================================
//  KYA SEEKHNA HAI
// ============================================================
//  Dangling pointer = a pointer whose target's lifetime has ENDED
//  (local went out of scope / function returned / delete'd / container
//  reallocated). Dereferencing it (read OR write) is UB.
//
//  READ  -> stale value now, or garbage, or crash.
//  WRITE -> corrupts whatever now occupies that memory -- a delayed,
//           far-away, hard-to-find bug.
//
//  The #1 defense: ownership + RAII (std::vector, std::unique_ptr,
//  std::shared_ptr -- folders 14, 17). Raw owning pointers are avoided
//  in modern C++.
// ============================================================
