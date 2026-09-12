// 06_futures.cpp
// ============================================================
// std::future / std::promise / std::async / std::packaged_task —
// "ek value jo baad mein milegi", aur exception propagation.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -pthread 06_futures.cpp -o fu && ./fu
// ============================================================

#include <cstdio>
#include <future>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

static long slow_sum(const std::vector<long>& v) {
    std::this_thread::sleep_for(20ms);           // pretend it's expensive
    return std::accumulate(v.begin(), v.end(), 0L);
}

int main() {
    // --------------------------------------------------------
    //  1. std::async — launch a task, get a future for its result
    // --------------------------------------------------------
    std::puts("1. std::async(launch::async):");
    std::vector<long> data(1000);
    std::iota(data.begin(), data.end(), 1);       // 1..1000

    std::future<long> f = std::async(std::launch::async, slow_sum, std::cref(data));
    // ... main thread yahan kuch aur kaam kar sakta ...
    std::printf("   doing other work while slow_sum runs...\n");
    long result = f.get();                         // blocks till ready
    std::printf("   sum(1..1000) = %ld\n\n", result);

    // --------------------------------------------------------
    //  2. launch::deferred — lazy, runs on get() in the CALLING thread
    // --------------------------------------------------------
    std::puts("2. std::async(launch::deferred) — lazy:");
    auto lazy = std::async(std::launch::deferred, [] {
        std::printf("   [deferred task] running NOW, on thread that called get()\n");
        return 42;
    });
    std::printf("   (not run yet)\n");
    std::printf("   lazy.get() = %d\n\n", lazy.get());

    // --------------------------------------------------------
    //  3. std::promise / std::future — manual hand-off between threads
    // --------------------------------------------------------
    std::puts("3. promise/future hand-off:");
    std::promise<std::string> p;
    std::future<std::string>  fut = p.get_future();

    std::thread producer([pr = std::move(p)]() mutable {
        std::this_thread::sleep_for(10ms);
        pr.set_value("computed on the worker thread");
    });
    std::printf("   got: \"%s\"\n\n", fut.get().c_str());
    producer.join();

    // --------------------------------------------------------
    //  4. Exception propagation — worker throws, get() re-throws
    // --------------------------------------------------------
    std::puts("4. exception through a future:");
    std::future<int> bad = std::async(std::launch::async, []() -> int {
        throw std::runtime_error("worker blew up");
    });
    try {
        int x = bad.get();
        std::printf("   got %d\n", x);
    } catch (const std::exception& e) {
        std::printf("   caught on main thread: %s\n\n", e.what());
    }

    // --------------------------------------------------------
    //  5. std::packaged_task — wrap a callable, run it, future for result
    //     (thread pools isse use karte — example 07)
    // --------------------------------------------------------
    std::puts("5. std::packaged_task:");
    std::packaged_task<long(long, long)> task([](long a, long b) { return a * b; });
    std::future<long> tf = task.get_future();
    std::thread runner(std::move(task), 6, 7);     // task ek alag thread pe chala
    std::printf("   6 * 7 = %ld\n", tf.get());
    runner.join();

    std::puts("\nSaar:");
    std::puts(" - future = 'value jo aane wali hai'. get() ek baar, blocks till ready.");
    std::puts(" - async(async) = naya thread; async(deferred) = get() pe hi, calling thread pe.");
    std::puts(" - promise/future = manual channel (worker set_value / set_exception).");
    std::puts(" - worker ka exception get() pe re-throw hota — clean cross-thread errors.");
    std::puts(" - packaged_task = callable + future; thread pools ka building block.");
    std::puts(" - ⚠️ std::async ka future destructor BLOCK karta hai (join) — surprise ho sakta.");
    return 0;
}
