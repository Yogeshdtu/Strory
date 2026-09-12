// 01_first_thread.cpp
// ============================================================
// Pehla thread: spawn, join, thread id, hardware_concurrency,
// arguments pass karna (by value copy hota hai — std::ref se reference).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -pthread 01_first_thread.cpp -o ft && ./ft
//   (MinGW pe -pthread optional hai, Linux pe zaroori)
// ============================================================

#include <cstdio>
#include <thread>
#include <vector>
#include <string>
#include <functional>   // std::ref

static void greet(int id, const std::string& tag) {
    std::printf("  [worker %d] tag=\"%s\"  tid=%zu\n",
                id, tag.c_str(),
                std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

// yeh function ek reference se counter badhata hai
static void add_to(long& counter, long times) {
    for (long i = 0; i < times; ++i) counter += 1;
}

int main() {
    std::printf("hardware_concurrency() = %u  (logical cores; hint, 0 = unknown)\n",
                std::thread::hardware_concurrency());
    std::printf("main tid = %zu\n\n",
                std::hash<std::thread::id>{}(std::this_thread::get_id()));

    // --------------------------------------------------------
    //  1. Ek thread — construct = start. join() = wait for finish.
    // --------------------------------------------------------
    std::puts("1. single thread:");
    std::thread t(greet, 1, std::string("hello"));
    // t abhi chal raha hai (concurrently with main)
    t.join();                                  // main yahan rukta hai jab tak t khatam
    std::puts("   joined\n");

    // ⚠️ join() (ya detach()) bhoole to ~thread -> std::terminate.

    // --------------------------------------------------------
    //  2. Lambda ko thread mein chalana
    // --------------------------------------------------------
    std::puts("2. lambda thread:");
    int captured = 42;
    std::thread t2([captured] {
        std::printf("  [lambda] captured value = %d\n", captured);
    });
    t2.join();
    std::puts("");

    // --------------------------------------------------------
    //  3. Kai threads ek vector mein
    // --------------------------------------------------------
    std::puts("3. 4 workers:");
    std::vector<std::thread> pool;
    for (int i = 0; i < 4; ++i)
        pool.emplace_back(greet, i, "batch");
    for (auto& th : pool) th.join();
    std::puts("");

    // --------------------------------------------------------
    //  4. Arguments COPY hote hain by default. Reference chahiye to
    //     std::ref(...) — warna thread ko ek copy milta hai.
    // --------------------------------------------------------
    std::puts("4. std::ref for pass-by-reference:");
    long counter = 0;
    // std::thread(add_to, counter, 1000) -> counter ki COPY jaati -> original 0 rehta.
    std::thread t4(add_to, std::ref(counter), 1000);   // ✅ ab asli counter
    t4.join();
    std::printf("   counter after 1 thread x1000 = %ld\n", counter);

    // NOTE: agar 2 threads bina sync ke counter++ karte -> RACE (example 02).

    // --------------------------------------------------------
    //  5. detach() — thread ko "chhod do", background mein chalta rahe.
    //     ⚠️ detached thread jo cheez use kare woh usse zyada jeeni chahiye.
    //     Yahan sirf syntax dikha rahe, join wala pattern prefer karo.
    // --------------------------------------------------------
    std::puts("\n5. detach (syntax only — join is safer):");
    std::thread t5([] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
    t5.detach();
    std::printf("   t5.joinable() after detach = %d\n", t5.joinable());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));   // detached thread khatam ho jaaye

    std::puts("\nSaar:");
    std::puts(" - std::thread construct = start; join() = wait; detach() = fire-and-forget");
    std::puts(" - join/detach bina ~thread -> std::terminate (jthread yeh fix karta — ex 09)");
    std::puts(" - args by value copy hote — std::ref se reference");
    std::puts(" - shared mutable data bina sync = data race = UB (ex 02)");
    return 0;
}
