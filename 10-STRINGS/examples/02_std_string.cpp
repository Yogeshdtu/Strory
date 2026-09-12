// 02_std_string.cpp
// ============================================================
// std::string -- apne chars ka maalik, apne aap badhta hai, safe API
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_std_string.cpp -o ss && ./ss
// ============================================================

#include <iostream>
#include <string>

int main() {
    // ============================================================
    //  1. Construction -- string banane ke tareeke
    // ============================================================
    // Har std::string object khud 32 bytes ka hai (libstdc++). Chhoti string (<= 15 chars)
    // usi ke andar reh jaati hai (SSO, lesson 04); lambi string heap pe jaati hai.
    std::cout << "===== 1. construction =====\n";
    std::string a = "hello";
    std::string b(5, 'x');                  // "xxxxx" -- 5 baar 'x'
    std::string c = a + " world";           // jodna: naya string banta hai (a nahi badalta)
    std::string d(a, 1, 3);                 // substring: "ell" (index 1 se, length 3) -- copy
    std::string e{"literal"};
    using namespace std::string_literals;
    std::string f = "with\0null"s;          // s-suffix -> length 9 (beech mein '\0' bhi chalega!)
    // Bina `s` suffix ke "with\0null" ek C-string maana jaata aur '\0' pe ruk jaata (length 4).

    std::cout << "  a = \"" << a << "\"  b = \"" << b << "\"  c = \"" << c << "\"\n";
    std::cout << "  d = \"" << d << "\"  f.size() = " << f.size()
              << "  (embedded '\\0' -- C-string yeh nahi kar sakti)\n";
    (void)e;

    // ============================================================
    //  2. size / length / empty / capacity
    // ============================================================
    // size() O(1) hai -- length object ke andar stored hai, strlen jaisa scan nahi.
    // capacity() = kitni jagah pehle se le rakhi hai; size se zyada ho sakti hai.
    std::cout << "\n===== 2. size / capacity =====\n";
    std::cout << "  c.size()     = " << c.size() << "  (== c.length())\n";
    std::cout << "  c.empty()    = " << std::boolalpha << c.empty() << "\n";
    std::cout << "  c.capacity() = " << c.capacity()
              << "  (kitna allocate hai -- size se bada ho sakta)\n";

    // ============================================================
    //  3. Indexing -- [] (check nahi) vs .at() (exception phenkta hai)
    // ============================================================
    // [] sabse tez hai par galat index pe UB. .at() har baar bounds check karta hai --
    // untrusted index (user input, network) pe .at() ya pehle khud check.
    std::cout << "\n===== 3. indexing =====\n";
    std::cout << "  a[0] = " << a[0] << "  a.front() = " << a.front()
              << "  a.back() = " << a.back() << "\n";
    try {
        std::cout << "  a.at(100) -> ";
        std::cout << a.at(100) << "\n";
    } catch (const std::out_of_range& ex) {
        std::cout << "throw std::out_of_range\n";
    }
    // a[a.size()]  -> '\0' (C++11 se PADHNA valid); a[a.size()+1] -> UB

    // ============================================================
    //  4. Modify -- apne aap badhta hai
    // ============================================================
    // Har += / append jab capacity se bahar jaaye tab naya bada buffer + copy hota hai
    // (lagbhag 2x badhta hai). Hot loop mein pehle reserve() karo (lesson 07).
    std::cout << "\n===== 4. modify =====\n";
    std::string s;
    s += "one";
    s += ' ';
    s.append("two");
    s.push_back('!');
    s.insert(0, ">> ");                     // shuru mein daalna = baaki chars ko khiskana (O(n))
    std::cout << "  built: \"" << s << "\"\n";
    s.replace(0, 3, "== ");
    std::cout << "  replaced: \"" << s << "\"\n";
    s.erase(0, 3);
    std::cout << "  erased: \"" << s << "\"\n";
    s.clear();                              // size 0, par buffer (capacity) wapas NAHI diya
    std::cout << "  after clear: empty=" << s.empty()
              << "  capacity still = " << s.capacity() << "  (memory retained)\n";

    // ============================================================
    //  5. Iteration -- range-for, index, iterators
    // ============================================================
    // Yeh BYTES pe chalta hai, characters pe nahi -- UTF-8 mein 'é' 2 bytes hai (lesson 08).
    std::cout << "\n===== 5. iteration =====\n";
    std::string word = "abc";
    std::cout << "  range-for: ";
    for (char ch : word) std::cout << ch << " ";
    std::cout << "\n  upper:     ";
    // toupper ko unsigned char dena zaroori -- negative char (non-ASCII byte) pe UB (lesson 09).
    for (char& ch : word) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    std::cout << word << "\n";

    // ============================================================
    //  6. Interop -- c_str() / data()
    // ============================================================
    // c_str() string ke ABHI wale buffer ka pointer deta hai. String badli ya mari to
    // yeh pointer dangling -- pointer ko string se zyada der mat rakho.
    std::cout << "\n===== 6. C interop =====\n";
    std::string path = "/tmp/file";
    const char* cpath = path.c_str();      // null-terminated -- C API ke liye
    std::cout << "  c_str() -> \"" << cpath << "\"  (guaranteed '\\0'-terminated)\n";
    std::cout << "  ⚠️ path modify/destroy ke baad cpath dangling (lesson 09)\n";

    // HFT note: symbols jaise chhote keys SSO mein aa jaate hain (allocation nahi). Lambi
    // strings hot path pe allocation laati hain -- wahan string_view / reserve + reuse.
    std::cout << "\n  std::string = C-string ke saare dangers gone: size tracked,\n"
                 "  auto-grow, bounds (.at), content ==, embedded '\\0' OK.\n"
                 "  Cost: heap allocation (chhoti strings ke liye SSO -- 03_sso_demo.cpp).\n";

    return 0;
}
