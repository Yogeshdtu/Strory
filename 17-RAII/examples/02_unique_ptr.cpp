// 02_unique_ptr.cpp
// ============================================================
// std::unique_ptr -- exclusive ownership, move-only, zero overhead
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_unique_ptr.cpp -o up && ./up
// ============================================================
//   unique_ptr<T>  = ek raw pointer + ek destructor jo `delete` karta hai.
//   - EK owner (copy allowed nahi, move allowed) -> ownership transfer hota hai
//   - scope end / reset / reassign  -> automatic `delete`
//   - sizeof(unique_ptr<T>) == sizeof(T*)  (default deleter -- zero overhead)
// ============================================================

#include <iostream>
#include <memory>
#include <vector>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::cout << "  Widget(" << id << ") ctor\n"; }
    ~Widget()                      { std::cout << "  Widget(" << id << ") dtor\n"; }
    void ping() const              { std::cout << "  Widget(" << id << ") ping\n"; }
};

// ownership function ke andar aata hai -> function ke end pe delete
void consume(std::unique_ptr<Widget> w) {
    std::cout << "  consume(): got Widget " << w->id << "\n";
}   // w scope se nikla -> Widget deleted

// ownership function se bahar jaata hai
std::unique_ptr<Widget> produce(int id) {
    return std::make_unique<Widget>(id);        // move out (RVO / implicit move)
}

int main() {
    std::cout << "=== 1. make_unique + automatic cleanup ===\n";
    {
        auto w = std::make_unique<Widget>(1);   // ✅ prefer make_unique (exception-safe, 1 line)
        w->ping();
        std::cout << "  w.get() = " << static_cast<const void*>(w.get()) << "\n";
    }   // Widget(1) dtor -- automatic

    std::cout << "\n=== 2. move-only: copy = compile error, move = transfer ===\n";
    auto a = std::make_unique<Widget>(2);
    // auto b = a;                               // ❌ compile ERROR -- unique_ptr non-copyable
    auto b = std::move(a);                       // ✅ ownership a -> b
    std::cout << "  after move: a is " << (a ? "set" : "null")
              << ", b owns Widget " << b->id << "\n";

    std::cout << "\n=== 3. pass ownership into a function ===\n";
    consume(std::move(b));                        // b -> consume's param -> deleted at consume end
    std::cout << "  after consume: b is " << (b ? "set" : "null") << "\n";

    std::cout << "\n=== 4. return ownership from a function ===\n";
    auto c = produce(3);
    c->ping();

    std::cout << "\n=== 5. reset / release ===\n";
    c.reset(new Widget{4});                       // purana Widget(3) delete, naya set
    Widget* raw = c.release();                    // ownership CHHOD do -- ab c null, raw ka delete AAP karo
    std::cout << "  released raw = Widget " << raw->id << ", c is " << (c ? "set" : "null") << "\n";
    delete raw;                                   // manual -- release() ke baad zimmedari aapki

    std::cout << "\n=== 6. unique_ptr<T[]> for arrays ===\n";
    {
        auto arr = std::make_unique<int[]>(5);    // delete[] karega (T[] specialization)
        for (std::size_t i = 0; i < 5; ++i) arr[i] = static_cast<int>(i * i);
        std::cout << "  arr[3] = " << arr[3] << "\n";
    }   // delete[] automatic

    std::cout << "\n=== 7. container of unique_ptr (polymorphism-safe) ===\n";
    std::vector<std::unique_ptr<Widget>> v;
    v.push_back(std::make_unique<Widget>(10));
    v.push_back(std::make_unique<Widget>(11));
    for (const auto& p : v) p->ping();
    std::cout << "  vector clearing...\n";
    // v scope end -> har element ka Widget deleted

    std::cout << "\n=== 8. sizeof ===\n";
    std::cout << "  sizeof(Widget*)             = " << sizeof(Widget*) << "\n";
    std::cout << "  sizeof(unique_ptr<Widget>)  = " << sizeof(std::unique_ptr<Widget>)
              << "   (default deleter -> ZERO overhead)\n";

    std::cout << "\n(main returning -- v, c ke destructors ab chalenge)\n";
    return 0;
}
