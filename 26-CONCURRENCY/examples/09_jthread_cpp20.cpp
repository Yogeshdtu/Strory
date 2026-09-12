// 09_jthread_cpp20.cpp
// ============================================================
// C++20 concurrency primitives:
//   std::jthread          — auto-join + cooperative cancellation
//   std::stop_token / stop_source / request_stop
//   std::latch            — one-shot countdown "sabhi pahunche?"
//   std::barrier          — reusable phase sync + completion function
//   std::counting_semaphore — permits (bounded resource / signalling)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -pthread 09_jthread_cpp20.cpp -o jt && ./jt
// ============================================================

#include <cstdio>
#include <thread>          // std::jthread
#include <stop_token>
#include <latch>
#include <barrier>
#include <semaphore>
#include <chrono>
#include <atomic>
#include <vector>

using namespace std::chrono_literals;

int main() {
    // --------------------------------------------------------
    //  1. std::jthread — RAII join, no std::terminate on scope exit
    // --------------------------------------------------------
    std::puts("1. std::jthread (auto-join):");
    {
        std::jthread jt([] {
            std::printf("   [jthread] running\n");
            std::this_thread::sleep_for(5ms);
        });
        // koi jt.join() nahi — jthread ka DESTRUCTOR join karta hai
    }   // <-- yahan join hua, scope-end pe
    std::puts("   (scope chhoda -> jthread apne aap join ho gaya)\n");

    // --------------------------------------------------------
    //  2. std::stop_token — cooperative cancellation
    // --------------------------------------------------------
    std::puts("2. stop_token / request_stop:");
    {
        std::jthread worker([](std::stop_token st) {
            long spins = 0;
            while (!st.stop_requested()) {         // poll the token
                ++spins;
                std::this_thread::sleep_for(1ms);
            }
            std::printf("   [worker] stop seen after %ld spins\n", spins);
        });
        std::this_thread::sleep_for(20ms);
        worker.request_stop();                     // ask it to finish
        // ~jthread also calls request_stop() then join() automatically
    }
    std::puts("");

    // --------------------------------------------------------
    //  3. std::latch — wait for N tasks to reach a point (one-shot)
    // --------------------------------------------------------
    std::puts("3. std::latch (start gun + finish line):");
    {
        constexpr int N = 4;
        std::latch ready{N};       // workers count down when initialized
        std::latch go{1};          // main opens the gate
        std::latch done{N};        // main waits for all to finish

        std::vector<std::jthread> ws;
        for (int i = 0; i < N; ++i)
            ws.emplace_back([&, i] {
                ready.count_down();          // "main, I'm ready"
                go.wait();                   // all start together
                std::printf("   [w%d] go!\n", i);
                done.count_down();
            });

        ready.wait();                        // all N ready
        std::printf("   all %d ready — firing\n", N);
        go.count_down();                     // release everyone at once
        done.wait();                         // all finished
        std::printf("   all done\n\n");
    }

    // --------------------------------------------------------
    //  4. std::barrier — reusable, with a completion function per phase
    // --------------------------------------------------------
    std::puts("4. std::barrier (3 phases):");
    {
        constexpr int N = 3;
        std::atomic<int> phase{0};
        std::barrier sync(N, [&phase] {      // runs ONCE when all arrive, before release
            std::printf("   --- phase %d complete ---\n", phase.fetch_add(1) + 1);
        });

        std::vector<std::jthread> ws;
        for (int i = 0; i < N; ++i)
            ws.emplace_back([&, i] {
                for (int p = 0; p < 3; ++p) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2 * (i + 1)));
                    sync.arrive_and_wait();      // wait for the other N-1
                }
            });
    }   // jthreads auto-join
    std::puts("");

    // --------------------------------------------------------
    //  5. std::counting_semaphore — limit concurrency to K
    // --------------------------------------------------------
    std::puts("5. counting_semaphore<3> — max 3 in the 'pool' at once:");
    {
        std::counting_semaphore<3> slots{3};
        std::atomic<int> in_use{0}, max_seen{0};

        std::vector<std::jthread> ws;
        for (int i = 0; i < 10; ++i)
            ws.emplace_back([&] {
                slots.acquire();                        // take a permit (blocks if 0)
                int cur = in_use.fetch_add(1) + 1;
                int prev = max_seen.load();
                while (cur > prev && !max_seen.compare_exchange_weak(prev, cur)) {}
                std::this_thread::sleep_for(5ms);
                in_use.fetch_sub(1);
                slots.release();                        // give the permit back
            });
        for (auto& w : ws) w.join();
        std::printf("   max concurrent = %d  (never exceeded 3)\n", max_seen.load());
    }

    std::puts("\nSaar:");
    std::puts(" - jthread: RAII join + stop_token. std::thread ke jhamele khatam.");
    std::puts(" - latch: one-shot countdown (start-together / wait-for-all).");
    std::puts(" - barrier: reusable phase sync + a completion callback per phase.");
    std::puts(" - counting_semaphore: K permits — bounded concurrency / signalling.");
    return 0;
}
