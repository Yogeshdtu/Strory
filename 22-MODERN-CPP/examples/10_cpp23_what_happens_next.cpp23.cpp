// 10_cpp23_what_happens_next.cpp23.cpp
// ============================================================
// Lesson 15 ke "What happens next?" sawaalon ka jawab-program.
// PEHLE lesson mein har sawaal ka jawab khud likho, PHIR isse chalao.
// ============================================================
//   g++ -std=c++23 -Wall -Wextra 10_cpp23_what_happens_next.cpp23.cpp -o whn -lstdc++exp && ./whn
// ============================================================

#include <flat_map>
#include <functional>
#include <generator>
#include <memory>
#include <print>
#include <string>
#include <utility>

struct Holder {
    std::string data = "order-42";
    template <class Self>
    auto&& value(this Self&& self) { return std::forward<Self>(self).data; }
};

// Har step pe print karta hai -- taaki dikhe body KAB chalti hai.
std::generator<int> ticks() {
    std::println("  [gen] body started");
    for (int i = 1; i <= 3; ++i) {
        std::println("  [gen] about to yield {}", i);
        co_yield i;                                   // yahan ruk jaata hai, caller ko i milta hai
        std::println("  [gen] resumed after {}", i);  // agle ++it pe yahan se chalu
    }
    std::println("  [gen] body finished");
}

int main() {
    // ============================================================
    //  Q1. std::move(h).value() ke baad h.data mein kya bacha?
    // ============================================================
    std::println("Q1) move out through deducing this");
    Holder h;
    std::string s = std::move(h).value();             // Self = Holder -> rvalue -> string MOVE
    // Moved-from string "valid but unspecified" hai. libstdc++ pe aksar khaali milti hai,
    // par standard yeh guarantee NAHI karta -- bharosa mat karo.
    std::println("  s=\"{}\"  h.data=\"{}\" (size {})", s, h.data, h.data.size());
    Holder h2;
    std::string& ref = h2.value();                    // lvalue object -> string& milta hai
    ref += "-X";                                      // reference se asli data badla
    std::println("  lvalue value() returned a reference -> h2.data=\"{}\"", h2.data);

    // ============================================================
    //  Q2. Generator ki body kab chalti hai?
    // ============================================================
    std::println("\nQ2) generator laziness");
    auto g = ticks();                                 // body abhi NAHI chali
    std::println("  after ticks() call");
    auto it = g.begin();                              // ab body pehle co_yield tak chali
    std::println("  after begin(): *it={}", *it);
    ++it;                                             // resume -> agle co_yield tak
    std::println("  after ++it: *it={}", *it);

    // ============================================================
    //  Q3. Moved-from move_only_function -- call karein ya nahi?
    // ============================================================
    std::println("\nQ3) moved-from move_only_function");
    std::move_only_function<int()> task = [p = std::make_unique<int>(7)] { return *p; };
    auto t2 = std::move(task);
    // Khaali move_only_function ko CALL karna UB hai (std::function jaisa exception nahi).
    // Isliye pehle bool check.
    if (task) std::println("  task still callable: {}", task());
    else      std::println("  task empty");
    std::println("  bool(t2)={}  t2()={}", static_cast<bool>(t2), t2());

    // ============================================================
    //  Q4. flat_map::emplace -- position aur duplicate key
    // ============================================================
    std::println("\nQ4) flat_map emplace return + position");
    std::flat_map<int, int> m{{10, 1}, {30, 3}};
    auto [pos, inserted] = m.emplace(20, 2);          // sorted jagah pe -- 10 aur 30 ke beech
    std::println("  inserted={} key={} index={}", inserted, pos->first, pos - m.begin());
    auto [pos2, inserted2] = m.emplace(20, 99);       // key pehle se hai -> kuch nahi badlega
    std::println("  second emplace(20,99): inserted={} value={}", inserted2, pos2->second);
}
