// 02_std_string.cpp
// ============================================================
// std::string -- owns its chars, grows automatically, safe API
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_std_string.cpp -o ss && ./ss
// ============================================================

#include <iostream>
#include <string>

int main() {
    // ============================================================
    //  1. Construction
    // ============================================================
    std::cout << "===== 1. construction =====\n";
    std::string a = "hello";
    std::string b(5, 'x');                  // "xxxxx"
    std::string c = a + " world";           // concatenation
    std::string d(a, 1, 3);                 // substring: "ell" (from index 1, len 3)
    std::string e{"literal"};
    using namespace std::string_literals;
    std::string f = "with\0null"s;          // s-suffix -> length 9 (embedded '\0' OK!)

    std::cout << "  a = \"" << a << "\"  b = \"" << b << "\"  c = \"" << c << "\"\n";
    std::cout << "  d = \"" << d << "\"  f.size() = " << f.size()
              << "  (embedded '\\0' -- C-string yeh nahi kar sakti)\n";
    (void)e;

    // ============================================================
    //  2. size / length / empty / capacity
    // ============================================================
    std::cout << "\n===== 2. size / capacity =====\n";
    std::cout << "  c.size()     = " << c.size() << "  (== c.length())\n";
    std::cout << "  c.empty()    = " << std::boolalpha << c.empty() << "\n";
    std::cout << "  c.capacity() = " << c.capacity()
              << "  (kitna allocate hai -- size se bada ho sakta)\n";

    // ============================================================
    //  3. Indexing -- [] (unchecked) vs .at() (throws)
    // ============================================================
    std::cout << "\n===== 3. indexing =====\n";
    std::cout << "  a[0] = " << a[0] << "  a.front() = " << a.front()
              << "  a.back() = " << a.back() << "\n";
    try {
        std::cout << "  a.at(100) -> ";
        std::cout << a.at(100) << "\n";
    } catch (const std::out_of_range& ex) {
        std::cout << "throw std::out_of_range\n";
    }
    // a[a.size()]  -> '\0' (valid to READ, C++11+); a[a.size()+1] -> UB

    // ============================================================
    //  4. Modify -- grows automatically
    // ============================================================
    std::cout << "\n===== 4. modify =====\n";
    std::string s;
    s += "one";
    s += ' ';
    s.append("two");
    s.push_back('!');
    s.insert(0, ">> ");
    std::cout << "  built: \"" << s << "\"\n";
    s.replace(0, 3, "== ");
    std::cout << "  replaced: \"" << s << "\"\n";
    s.erase(0, 3);
    std::cout << "  erased: \"" << s << "\"\n";
    s.clear();
    std::cout << "  after clear: empty=" << s.empty()
              << "  capacity still = " << s.capacity() << "  (memory retained)\n";

    // ============================================================
    //  5. Iteration -- range-for, indexed, iterators
    // ============================================================
    std::cout << "\n===== 5. iteration =====\n";
    std::string word = "abc";
    std::cout << "  range-for: ";
    for (char ch : word) std::cout << ch << " ";
    std::cout << "\n  upper:     ";
    for (char& ch : word) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    std::cout << word << "\n";

    // ============================================================
    //  6. Interop -- c_str() / data()
    // ============================================================
    std::cout << "\n===== 6. C interop =====\n";
    std::string path = "/tmp/file";
    const char* cpath = path.c_str();      // null-terminated -- C API ke liye
    std::cout << "  c_str() -> \"" << cpath << "\"  (guaranteed '\\0'-terminated)\n";
    std::cout << "  ⚠️ path modify/destroy ke baad cpath dangling (lesson 09)\n";

    std::cout << "\n  std::string = C-string ke saare dangers gone: size tracked,\n"
                 "  auto-grow, bounds (.at), content ==, embedded '\\0' OK.\n"
                 "  Cost: heap allocation (chhoti strings ke liye SSO -- 03_sso_demo.cpp).\n";

    return 0;
}
