// 02_class_templates.cpp
// ============================================================
// Class templates -- a generic fixed-capacity Stack:
// member functions, non-type param (capacity), CTAD (C++17),
// member function templates, and a full/partial specialization.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_class_templates.cpp -o ct && ./ct
// ============================================================

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

// ---- generic fixed-capacity stack ----
template <class T, std::size_t Cap>
class FixedStack {
    T           data_[Cap];
    std::size_t size_ = 0;
public:
    bool empty() const { return size_ == 0; }
    bool full()  const { return size_ == Cap; }
    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return Cap; }

    void push(const T& x) {
        if (full()) throw std::overflow_error("FixedStack full");
        data_[size_++] = x;
    }
    void pop() {
        if (empty()) throw std::underflow_error("FixedStack empty");
        --size_;
    }
    T&       top()       { return data_[size_ - 1]; }
    const T& top() const { return data_[size_ - 1]; }

    // ---- a MEMBER function template: push a range of any iterator type ----
    template <class It>
    void push_range(It first, It last) {
        for (; first != last; ++first) push(*first);
    }
};

// ---- CTAD guide (C++17): let `FixedStack s{1,2,3}` deduce <int, 3> ... ----
// (for a variadic ctor; here we keep it explicit -- CTAD shown below with a pair)

// ---- a class template with a specialization ----
template <class T>
struct TypeName { static const char* get() { return "unknown"; } };
template <> struct TypeName<int>         { static const char* get() { return "int"; } };
template <> struct TypeName<double>      { static const char* get() { return "double"; } };
template <> struct TypeName<std::string> { static const char* get() { return "std::string"; } };

// ---- PARTIAL specialization: anything that is a pointer ----
template <class T>
struct TypeName<T*> { static const char* get() { return "a pointer"; } };

int main() {
    std::printf("=== 1. FixedStack<int, 4> ===\n");
    FixedStack<int, 4> s;
    s.push(10); s.push(20); s.push(30);
    std::printf("  size=%zu cap=%zu top=%d\n", s.size(), s.capacity(), s.top());
    s.pop();
    std::printf("  after pop: top=%d\n", s.top());

    std::printf("\n=== 2. member function template (push_range) ===\n");
    int src[] = {1, 2, 3};
    FixedStack<int, 8> s2;
    s2.push_range(std::begin(src), std::end(src));
    std::printf("  pushed 3, size=%zu top=%d\n", s2.size(), s2.top());

    std::printf("\n=== 3. overflow is caught ===\n");
    try {
        FixedStack<int, 2> tiny;
        tiny.push(1); tiny.push(2); tiny.push(3);   // throws
    } catch (const std::exception& e) {
        std::printf("  caught: %s\n", e.what());
    }

    std::printf("\n=== 4. CTAD (class template argument deduction) ===\n");
    std::pair p{42, std::string("answer")};          // -> std::pair<int, std::string>, no <...> needed
    std::printf("  std::pair p{42, \"answer\"} -> (%d, \"%s\")\n", p.first, p.second.c_str());

    std::printf("\n=== 5. full + partial specialization ===\n");
    std::printf("  TypeName<int>         : %s\n", TypeName<int>::get());
    std::printf("  TypeName<double>      : %s\n", TypeName<double>::get());
    std::printf("  TypeName<std::string> : %s\n", TypeName<std::string>::get());
    std::printf("  TypeName<char>        : %s\n", TypeName<char>::get());        // -> "unknown" (primary)
    std::printf("  TypeName<int*>        : %s\n", TypeName<int*>::get());        // -> "a pointer" (partial)
    std::printf("  TypeName<double*>     : %s\n", TypeName<double*>::get());     // -> "a pointer"

    std::printf(
        "\n"
        "  Each instantiation (FixedStack<int,4>, FixedStack<int,8>, ...) is a\n"
        "  SEPARATE class the compiler generates. Cap is a compile-time constant, so\n"
        "  the array is inline (no heap) and loops over it can be unrolled.\n");
    return 0;
}
