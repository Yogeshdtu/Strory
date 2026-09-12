// 03_endl_benchmark.cpp
// ============================================================
// \n vs endl -- REAL numbers ke saath
// ============================================================
// YEH IS FOLDER KA SABSE IMPORTANT EXAMPLE HAI.
//
// endl = '\n' + FLUSH. Aur har flush ek write() SYSCALL hai (~500-2000 ns).
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 03_endl_benchmark.cpp -o bench
//   ./bench > /dev/null          <- output discard, sirf timing dekho
//
// Syscalls count karo:
//   strace -c -e trace=write ./bench > /dev/null
// ============================================================

#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>

int main() {
    const int N = 100000;

    // Timing results ko `cerr` pe bhejenge, kyunki `cout` ko hum
    // /dev/null pe redirect kar rahe hain (measurement ke liye).

    // ============================================================
    //  TEST 1: "\n" -- sirf newline character
    // ============================================================
    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        std::cout << i << "\n";
    }
    std::cout.flush();                    // fair comparison ke liye end mein flush
    auto t2 = std::chrono::steady_clock::now();

    // ============================================================
    //  TEST 2: std::endl -- newline + FLUSH (har baar syscall)
    // ============================================================
    for (int i = 0; i < N; ++i) {
        std::cout << i << std::endl;      // ⚠️ har line pe write() syscall
    }
    auto t3 = std::chrono::steady_clock::now();

    // ============================================================
    //  TEST 3: sync_with_stdio(false) + "\n"
    // ============================================================
    // Default se C++ streams C ke stdio ke saath SYNCHRONIZED hote hain,
    // taaki aap printf aur cout mix kar sako. Uski cost hoti hai.
    // Sync off karne se cout apna buffer use karta hai -> 2-5x faster.
    //
    // ⚠️ Iske baad printf aur cout MIX MAT karna -- order galat ho sakta hai.
    std::ios::sync_with_stdio(false);
    auto t4 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        std::cout << i << "\n";
    }
    std::cout.flush();
    auto t5 = std::chrono::steady_clock::now();

    // ============================================================
    //  TEST 4: Ek string mein build karo, EK baar likho
    // ============================================================
    // Sabse tez -- sirf ek write() syscall.
    auto t6 = std::chrono::steady_clock::now();
    {
        std::string buffer;
        buffer.reserve(static_cast<std::size_t>(N) * 8);   // pehle se allocate
        for (int i = 0; i < N; ++i) {
            buffer += std::to_string(i);
            buffer += '\n';
        }
        std::cout << buffer;              // EK operator<< call
        std::cout.flush();
    }
    auto t7 = std::chrono::steady_clock::now();

    // ============================================================
    //  RESULTS
    // ============================================================
    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    const double tNewline  = ms(t1, t2);
    const double tEndl     = ms(t2, t3);
    const double tSyncOff  = ms(t4, t5);
    const double tBuffered = ms(t6, t7);

    std::cerr << std::fixed << std::setprecision(2);
    std::cerr << "\n══════════════════════════════════════════════════\n";
    std::cerr << "  " << N << " lines ka output\n";
    std::cerr << "══════════════════════════════════════════════════\n";
    std::cerr << std::left;
    std::cerr << std::setw(34) << "1. cout << i << \"\\n\""
              << std::right << std::setw(9) << tNewline << " ms\n";
    std::cerr << std::left << std::setw(34) << "2. cout << i << std::endl"
              << std::right << std::setw(9) << tEndl << " ms";
    std::cerr << "   <- " << (tEndl / tNewline) << "x SLOWER\n";
    std::cerr << std::left << std::setw(34) << "3. sync_with_stdio(false) + \"\\n\""
              << std::right << std::setw(9) << tSyncOff << " ms";
    std::cerr << "   <- " << (tNewline / tSyncOff) << "x faster\n";
    std::cerr << std::left << std::setw(34) << "4. build string, one write"
              << std::right << std::setw(9) << tBuffered << " ms";
    std::cerr << "   <- " << (tNewline / tBuffered) << "x faster\n";
    std::cerr << "══════════════════════════════════════════════════\n";

    std::cerr << "\nKYA SEEKHA:\n";
    std::cerr << "  * endl har line pe write() SYSCALL karta hai\n";
    std::cerr << "  * Ek syscall ~500-2000 nanoseconds ka hota hai\n";
    std::cerr << "  * " << N << " syscalls = " << (N * 1000.0 / 1e6)
              << " ms sirf syscall overhead\n";
    std::cerr << "\nRULE: 99% cases mein \"\\n\" use karo, endl nahi.\n";
    std::cerr << "      endl sirf tab jab CRASH se pehle output pakka chahiye.\n";

    std::cerr << "\nHFT RELEVANCE:\n";
    std::cerr << "  Tick-to-trade budget: ~5 microseconds (5000 ns)\n";
    std::cerr << "  Ek endl syscall: ~500-2000 ns\n";
    std::cerr << "  -> Ek log line aapka 10-40% budget kha jaati hai!\n";
    std::cerr << "  Isliye HFT mein logging ASYNCHRONOUS hoti hai:\n";
    std::cerr << "    hot path -> lock-free queue mein RAW values daalo (~20ns)\n";
    std::cerr << "    logger thread -> format karo aur likho (hot path ko farak nahi)\n";
    std::cerr << "  Folder 41 mein poora banayenge.\n\n";

    std::cerr << "Syscalls count karne ke liye chalao:\n";
    std::cerr << "  strace -c -e trace=write ./bench > /dev/null\n\n";

    return 0;
}
