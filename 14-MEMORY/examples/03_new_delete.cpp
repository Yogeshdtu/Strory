// 03_new_delete.cpp
// ============================================================
// Manual memory: new / delete, new[] / delete[], nothrow, ctor+dtor
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_new_delete.cpp -o nd && ./nd
// ============================================================
//   new T        -> (1) memory allocate  (2) T ka constructor chalao   -> T*
//   delete p     -> (1) T ka destructor chalao  (2) memory free
//   new T[n]     -> memory + n constructors ;  delete[] p -> n destructors + free
//
//   ⚠️ new  ke saath delete   ,  new[] ke saath delete[]  -- mix karna UB.
// ============================================================

#include <iostream>
#include <new>          // std::nothrow

struct Tagged {
    int id;
    Tagged() : id(0)            { std::cout << "  ctor Tagged(" << id << ")\n"; }
    explicit Tagged(int v) : id(v) { std::cout << "  ctor Tagged(" << id << ")\n"; }
    ~Tagged()                  { std::cout << "  dtor Tagged(" << id << ")\n"; }
};

int main() {
    // ---- 1. single object ----
    std::cout << "=== new / delete (single) ===\n";
    int* n = new int(42);              // allocate + init
    std::cout << "  *n = " << *n << "   @ " << static_cast<const void*>(n) << "\n";
    delete n;                          // free
    n = nullptr;                       // achhi aadat -- dangling se bachne ke liye (folder 12)

    // ---- 2. object with ctor/dtor ----
    std::cout << "\n=== new runs the constructor, delete runs the destructor ===\n";
    Tagged* t = new Tagged(7);         // "ctor Tagged(7)"
    std::cout << "  t->id = " << t->id << "\n";
    delete t;                          // "dtor Tagged(7)"

    // ---- 3. arrays ----
    std::cout << "\n=== new[] / delete[] ===\n";
    const std::size_t k = 3;
    int* arr = new int[k]{10, 20, 30};
    for (std::size_t i = 0; i < k; ++i) std::cout << "  arr[" << i << "] = " << arr[i] << "\n";
    delete[] arr;                      // NOTE the []

    std::cout << "\n=== new Tagged[3] -> 3 ctors, delete[] -> 3 dtors ===\n";
    Tagged* ts = new Tagged[3];        // 3x "ctor Tagged(0)"
    ts[1].id = 99;
    delete[] ts;                       // 3x "dtor" (reverse order)

    // ---- 4. nothrow ----
    std::cout << "\n=== nothrow new (failure pe nullptr, exception nahi) ===\n";
    int* maybe = new (std::nothrow) int[8];
    if (maybe) { std::cout << "  mila\n"; delete[] maybe; }
    else       { std::cout << "  alloc fail -> nullptr\n"; }

    // ---- 5. galtiyan (comment mein -- run mat karo) ----
    // int* p = new int[4];  delete p;      // ⚠️ UB -- new[] ko delete (bina []) se free kiya
    // int* q = new int;     delete[] q;    // ⚠️ UB -- ulta
    // delete n;  delete n;                 // ⚠️ double-free (file 06)
    // Tagged* leak = new Tagged(1);        // ⚠️ delete nahi -> leak (file 05)

    std::cout <<
        "\n"
        "  Rule: har `new` ke liye THEEK ek `delete` (aur `new[]` <-> `delete[]`).\n"
        "  Practice mein: raw new/delete avoid karo -- std::vector / std::unique_ptr\n"
        "  (RAII, folder 17) yeh khud handle karte hain.\n";
    return 0;
}
