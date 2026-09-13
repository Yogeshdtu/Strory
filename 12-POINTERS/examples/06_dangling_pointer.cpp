// 06_dangling_pointer.cpp
// ============================================================
//  ⚠️  IS FILE MEIN JAAN-BOOJH KAR DANGLING-POINTER BUGS HAIN  ⚠️
// ============================================================
//  COMPILE ho jaata hai (3 intentional warnings ke saath). Chalane pe: kabhi
//  "kaam kar jaata hai" (purani value), kabhi garbage, kabhi crash --
//  yeh UNDEFINED BEHAVIOUR hai.
//
//  PAKADNE KA TAREEKA:
//   - AddressSanitizer (best):  Linux/macOS/Clang:
//       g++ -std=c++20 -fsanitize=address,undefined -g 06_dangling_pointer.cpp -o dp && ./dp write
//     -> "heap-use-after-free" / "stack-use-after-return" exact line ke saath.
//   ⚠️ MinGW-w64 (yeh Windows toolchain) mein libasan NAHI -> WSL/Linux use karo.
//   - -Wreturn-local-addr / -Wdangling-pointer (dono -Wall mein) -- kuch cases
//     compile-time pe. GCC 16.2 -Wall is file pe 3 warnings deta hai (BUG 1, 1b, 4).
//
//  ⚠️ `make folder` / `checkall` sirf COMPILE karte hain -> "OK" dikhega.
//
//  ⚠️ GCC 16.2 KA SURPRISE (BUG 1): `return &x;` ko compiler `return nullptr;` bana
//  deta hai -- -O0 pe bhi (assembly: `mov eax, 0`). Is file ka purana version
//  `*danglingLocal()` padhta tha -> seedha Segmentation fault (exit 139) aur
//  BUG 2-4 kabhi chalte hi nahi the. Isliye BUG 1 ab sirf pointer ki value
//  dikhata hai, aur BUG 1b (out-parameter) asli "stale stack" dikhata hai.
// ============================================================

#include <iostream>
#include <string_view>
#include <vector>

// ---- BUG 1: local ka pointer return -- use-after-return ----
int* danglingLocal() {
    int x = 42;
    return &x;                    // ⚠️ function return hote hi x khatam
                                  //    (-Wreturn-local-addr; GCC yahan nullptr return karta hai)
}

// ---- BUG 1b: wahi galti, par address out-parameter se bahar gaya ----
void danglingOut(int*& out) {
    int x = 42;
    out = &x;                     // ⚠️ -Wdangling-pointer: storing the address of local variable 'x'
}                                 //    yahan compiler address ko null nahi karta -> asli dangling

// ---- BUG 3: vector ke andar ka pointer, phir vector reallocate ----
void vectorReallocDemo() {
    std::vector<int> v = {1, 2, 3};
    int* p = &v[0];              // p -> v ka abhi wala buffer
    std::cout << "  pehle: *p = " << *p << "\n";
    for (int i = 0; i < 1000; ++i) v.push_back(i);   // ⚠️ reallocation -> purana buffer free -> p dangling
    std::cout << "  1000 push_back ke baad: *p = " << *p
              << "   (⚠️ p ab free ho chuki memory mein point karta hai)\n";
}

int main(int argc, char** argv) {
    // `./dp write` -> BUG 2 mein free memory pe WRITE bhi karo (neeche dekho)
    const bool doWrite = (argc > 1 && std::string_view(argv[1]) == "write");

    // ============================================================
    //  BUG 1: use-after-return
    // ============================================================
    std::cout << "===== BUG 1: local ka pointer return =====\n";
    int* p1 = danglingLocal();
    std::cout << "  p1 = " << static_cast<const void*>(p1)
              << (p1 == nullptr ? "   <- nullptr! GCC ne &x ki jagah 0 return kiya\n"
                                : "   <- (is compiler ne address wapas diya)\n");
    // std::cout << *p1;          // 💥 GCC 16.2 pe: null deref -> crash. Doosre compiler pe: stale value.
    //                            //    Dono UB hain -- "kya hoga" compiler tay karta hai, aap nahi.

    // ============================================================
    //  BUG 1b: use-after-return, out-parameter ke through
    // ============================================================
    std::cout << "\n===== BUG 1b: out-parameter se bahar gaya local address =====\n";
    int* p1b = nullptr;
    danglingOut(p1b);
    std::cout << "  *p1b = " << *p1b << "   (⚠️ 42 ki ummeed? frame khatam -- jo mila kismat hai)\n";
    danglingOut(p1b);                         // dobara call -- wahi stack slot reuse
    std::cout << "  *p1b = " << *p1b << "   (⚠️ badal sakta hai -- pure UB)\n";

    // ============================================================
    //  BUG 2: use-after-free (heap)
    // ============================================================
    std::cout << "\n===== BUG 2: use-after-free =====\n";
    int* p2 = new int(99);
    std::cout << "  *p2 = " << *p2 << "\n";
    delete p2;                                // memory allocator ko wapas
    std::cout << "  delete ke baad: *p2 = " << *p2 << "   (⚠️ free ho chuki -- garbage/purani value)\n";
    if (doWrite) {
        // ⚠️ free memory mein likhna -- allocator ki free-list corrupt.
        // GCC 16.2 / MinGW, -O0 pe naapa (5/5 baar): yahan kuch nahi hota, crash AAGE
        // BUG 3 ke vector allocation mein aata hai (Segmentation fault, exit 139) --
        // "der se, door kahin" wala bug, apni aankhon se. -O2 pe GCC is dead store ko
        // hata deta hai aur crash gayab -- UB ka result flags pe depend karta hai.
        *p2 = 7;
        std::cout << "  free memory mein 7 likha -- ASan yahin rok deta; yahan crash aage aayega\n";
    } else {
        std::cout << "  (free memory pe WRITE dekhna hai? `./dp write` chalao -- crash BUG 3 mein aayega)\n";
    }
    // delete p2;                             // ⚠️ double-free -- yeh bhi UB

    // ============================================================
    //  BUG 3: container reallocation ke baad dangling
    // ============================================================
    std::cout << "\n===== BUG 3: vector reallocation =====\n";
    vectorReallocDemo();

    // ============================================================
    //  BUG 4: scope khatam hone ke baad dangling
    // ============================================================
    std::cout << "\n===== BUG 4: pointer apne target ke scope se zyada jiya =====\n";
    int* p4 = nullptr;
    {
        int temp = 123;
        p4 = &temp;
    }                                        // temp yahan destroy
    std::cout << "  *p4 = " << *p4 << "   (⚠️ temp ka scope khatam -- dangling; -Wdangling-pointer)\n";

    // ============================================================
    //  FIXES
    // ============================================================
    std::cout << "\n-----------------------------------------------\n";
    std::cout <<
        "FIXES:\n"
        "  * &local return mat karo -- value se return karo (RVO, folder 08), ya out-param mein VALUE do.\n"
        "  * delete p; ke baad p = nullptr; (chupa UAF -> saaf null-deref crash).\n"
        "  * Behtar: raw new/delete hi nahi -- std::vector / std::unique_ptr (folder 14, 17).\n"
        "  * vector: pehle reserve(), ya size badalne ke baad &v[i] dobara lo (ya index rakho).\n"
        "  * Pointer apne target se zyada nahi jeena chahiye. Lifetimes match karo.\n"
        "  * CI: -fsanitize=address,undefined (Linux) ; debug: -D_GLIBCXX_ASSERTIONS.\n";

    return 0;
}

// ============================================================
//  KYA SEEKHNA HAI
// ============================================================
//  Dangling pointer = aisa pointer jiske target ki lifetime KHATAM ho gayi
//  (local scope se bahar / function return / delete / container
//  reallocate). Use deref karna (read YA write) UB hai.
//
//  READ  -> abhi purani value, ya garbage, ya crash. (BUG 1: GCC ne address hi
//           null kar diya -> pakka crash. BUG 1b: 42 ki jagah kuch aur.)
//  WRITE -> ab us memory mein jo bhi hai use corrupt -- der se, door kahin,
//           mushkil se milne wala bug.
//
//  UB ka "result" compiler aur flags tay karte hain -- ek hi galti ek compiler
//  pe stale value, doosre pe nullptr crash. Kisi bhi result pe bharosa mat karo.
//
//  Sabse bada bachav: ownership + RAII (std::vector, std::unique_ptr,
//  std::shared_ptr -- folders 14, 17). Modern C++ mein raw owning pointers
//  se bacha jaata hai.
// ============================================================
