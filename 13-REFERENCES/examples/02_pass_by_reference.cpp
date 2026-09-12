// 02_pass_by_reference.cpp
// ============================================================
// Function parameters: by value vs by pointer vs by reference
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_pass_by_reference.cpp -o pbr && ./pbr
// ============================================================
//   by value      -> function ko ek COPY milti hai; caller ka original safe (aur unchanged)
//   by pointer    -> caller ka object badal sakte ho; call site pe &, body mein * chahiye
//   by reference  -> caller ka object badal sakte ho; syntax normal variable jaisa
//   by const&     -> bada object bina copy ke PADHO (write nahi kar sakte)
// ============================================================

#include <iostream>
#include <vector>

// by value -- caller ka variable NAHI badalta (copy pe kaam hota hai)
void addOne_byValue(int n) { n += 1; }

// by pointer -- badalta hai, par call site pe & aur body mein * chahiye
void addOne_byPtr(int* n) { *n += 1; }

// by reference -- badalta hai, syntax saaf
void addOne_byRef(int& n) { n += 1; }

// output parameters -- ek call se ek se zyada "return"
void divmod(int a, int b, int& quot, int& rem) {
    quot = a / b;
    rem  = a % b;
}

// const& -- bada object bina copy ke padho (file 04)
long long sumOf(const std::vector<int>& v) {
    long long s = 0;
    for (int e : v) s += e;
    return s;
}

// swap -- classic pass-by-reference
void swapInts(int& a, int& b) {
    int t = a;
    a = b;
    b = t;
}

int main() {
    std::cout << "===== by value =====\n";
    int x = 5;
    addOne_byValue(x);
    std::cout << "  addOne_byValue(x);  x = " << x << "   (unchanged -- copy pe kaam hua)\n";

    std::cout << "\n===== by pointer =====\n";
    addOne_byPtr(&x);
    std::cout << "  addOne_byPtr(&x);   x = " << x << "   (changed -- & lagana pada)\n";

    std::cout << "\n===== by reference =====\n";
    addOne_byRef(x);
    std::cout << "  addOne_byRef(x);    x = " << x << "   (changed -- call site pe kuch extra nahi)\n";

    std::cout << "\n===== output parameters =====\n";
    int q = 0, rem = 0;
    divmod(17, 5, q, rem);
    std::cout << "  divmod(17, 5, q, rem)  ->  q = " << q << ", rem = " << rem << "\n";

    std::cout << "\n===== const& (bade object, zero copy) =====\n";
    std::vector<int> big(1000, 1);
    std::cout << "  sumOf(big) = " << sumOf(big) << "   (1000-int vector COPY nahi hua)\n";

    std::cout << "\n===== swap =====\n";
    int a = 1, b = 2;
    swapInts(a, b);
    std::cout << "  swapInts(a, b)  ->  a = " << a << ", b = " << b << "\n";

    std::cout <<
        "\n"
        "  badalna hai?          -> T&   (ya T* agar 'optional' chahiye)\n"
        "  sirf padhna hai, chhota? -> T   (by value -- int/double/char)\n"
        "  sirf padhna hai, bada?   -> const T&   (vector/string/struct)\n";
    return 0;
}
