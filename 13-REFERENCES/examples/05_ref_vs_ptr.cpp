// 05_ref_vs_ptr.cpp
// ============================================================
// Reference vs Pointer -- har farq, ek jagah
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_ref_vs_ptr.cpp -o rvp && ./rvp
// ============================================================

#include <iostream>
#include <vector>

void takesPtr(int* p) { if (p) *p += 1; }   // null check ZAROORI (p null ho sakta hai)
void takesRef(int& r) { r += 1; }           // hamesha valid maana jaata hai (caller ki zimmedari)

int main() {
    int a = 10, b = 20;

    // ---- 1. declaration / init ----
    int* p = &a;          // init optional (par uninitialized pointer khatarnaak)
    int& r = a;           // init MANDATORY -- 'int& r;' compile error

    // ---- 2. size / identity ----
    std::cout << "sizeof(p) = " << sizeof(p)
              << "  (pointer khud 8 bytes) ;  sizeof(r) = " << sizeof(r)
              << "  (referent ka size = int)\n";
    std::cout << "&a = " << &a << " ;  &r = " << &r
              << "  (SAME) ;  p = " << p << " ;  &p = " << &p << "  (p ka apna address)\n\n";

    // ---- 3. rebind ----
    p = &b;               // OK -- p ab b ko point karta hai
    r = b;                // yeh 'a = b' hai! r abhi bhi a ka alias hai
    std::cout << "after  p = &b;  r = b;   ->  a=" << a << " b=" << b
              << "  *p=" << *p << "  r=" << r << "\n";
    a = 10;               // reset

    // ---- 4. null ----
    p = nullptr;          // OK -- matlab "abhi kuch nahi"
    // int& rn = nullptr; // compile ERROR
    std::cout << "p = nullptr;  OK.   reference kabhi 'nothing' nahi ho sakti.\n";

    // ---- 5. use-site syntax ----
    p = &a;
    *p = 5;               // pointer: deref (*) chahiye
    r  = 6;               // reference: seedha naam (r abhi a ka alias -> a = 6)
    std::cout << "*p = 5;  then  r = 6;   ->  a = " << a << "\n";

    // ---- 6. arithmetic ----
    std::vector<int> v{1, 2, 3, 4};
    int* q = v.data();
    std::cout << "pointer arithmetic:  *(q + 2) = " << *(q + 2)
              << "   (reference pe '+2' ka koi matlab nahi)\n";

    // ---- 7. containers ----
    std::vector<int*> ptrs{&a, &b};       // OK -- pointers container mein
    // std::vector<int&> nope;            // compile ERROR -- references ka container nahi
    std::cout << "vector<int*> size = " << ptrs.size()
              << "   (vector<int&> allowed NAHI)\n";

    // ---- 8. as parameters ----
    int x = 100;
    takesPtr(&x);
    takesPtr(nullptr);                    // pointer version ko null bhej sakte ho
    takesRef(x);                          // reference version -- na &, na null
    std::cout << "takesPtr(&x) + takesPtr(nullptr) + takesRef(x)  ->  x = " << x << "\n";

    std::cout <<
        "\n--- summary --------------------------------------------------------\n"
        "                 pointer                 reference\n"
        "  init           optional                mandatory\n"
        "  rebind         yes  (p = &y)           no  (r = y  =>  referent mein copy)\n"
        "  null           yes  (nullptr)          no\n"
        "  arithmetic     yes  (p + n)            no\n"
        "  use syntax     *p , p->m               r , r.m\n"
        "  in containers  vector<T*>  OK          vector<T&>  illegal\n"
        "  sizeof         8  (the pointer)        sizeof(T)  (the referent)\n"
        "  best for       optional / rebindable   guaranteed-present alias\n";
    return 0;
}
