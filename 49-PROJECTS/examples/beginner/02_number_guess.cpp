// 02_number_guess.cpp  --  49-PROJECTS beginner P2
// ============================================================
// Higher/lower guessing game. Core logic is separated from I/O so it can
// be driven by a scripted input stream and asserted. <random> done right.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 02_number_guess.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdio>
#include <istream>
#include <optional>
#include <random>
#include <sstream>
#include <string>

namespace {

enum class Hint { TooLow, TooHigh, Correct };

Hint check(int guess, int secret) {
    if (guess < secret) return Hint::TooLow;
    if (guess > secret) return Hint::TooHigh;
    return Hint::Correct;
}

// Reads guesses from `in` until the secret is found or input runs out.
// Returns the number of attempts, or nullopt if the stream ended first.
struct Result { int attempts; bool solved; };

Result play(std::istream& in, int secret, int lo, int hi) {
    int attempts = 0;
    int guess = 0;
    while (in >> guess) {
        if (guess < lo || guess > hi) continue;      // ignore out-of-range noise
        ++attempts;
        if (check(guess, secret) == Hint::Correct) return {attempts, true};
    }
    // recover the stream from a non-numeric token so a real REPL could reprompt
    in.clear();
    return {attempts, false};
}

// The optimal strategy: binary search -> ceil(log2(range)) guesses worst case.
int solve_optimally(int secret, int lo, int hi) {
    int attempts = 0;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;          // overflow-safe midpoint
        ++attempts;
        const Hint h = check(mid, secret);
        if (h == Hint::Correct) return attempts;
        if (h == Hint::TooLow)  lo = mid + 1;
        else                    hi = mid - 1;
    }
    return attempts;                                  // unreachable if secret in range
}

int secret_in_range(int lo, int hi, std::uint32_t seed) {
    std::mt19937 rng(seed);                           // seeded -> reproducible in tests
    std::uniform_int_distribution<int> dist(lo, hi);  // NOT rand()%range (biased)
    return dist(rng);
}

} // namespace

int main() {
    // hint direction
    assert(check(30, 50) == Hint::TooLow);
    assert(check(70, 50) == Hint::TooHigh);
    assert(check(50, 50) == Hint::Correct);

    // scripted play: wrong guesses then the right one
    {
        std::istringstream in("10 90 50 42");
        const Result r = play(in, 42, 1, 100);
        assert(r.solved && r.attempts == 4);
    }
    // out-of-range tokens are ignored, don't count
    {
        std::istringstream in("500 -3 42");
        const Result r = play(in, 42, 1, 100);
        assert(r.solved && r.attempts == 1);
    }
    // input runs out before solving -> not solved, stream recovered
    {
        std::istringstream in("1 2 3");
        const Result r = play(in, 99, 1, 100);
        assert(!r.solved && r.attempts == 3);
    }
    // non-numeric input doesn't hang or crash
    {
        std::istringstream in("40 abc 50");
        const Result r = play(in, 77, 1, 100);
        assert(!r.solved);
    }

    // optimal strategy never exceeds ceil(log2(100)) = 7 for any secret in [1,100]
    for (int s = 1; s <= 100; ++s) assert(solve_optimally(s, 1, 100) <= 7);

    // <random> is deterministic given a seed, and stays in range
    for (std::uint32_t seed = 0; seed < 1000; ++seed) {
        const int v = secret_in_range(1, 100, seed);
        assert(v >= 1 && v <= 100);
    }
    assert(secret_in_range(1, 100, 12345) == secret_in_range(1, 100, 12345));

    std::puts("02_number_guess: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - check()/play() have NO cin/cout -> testable with std::istringstream.
//   - <random>: std::mt19937 + uniform_int_distribution, seeded. `rand()%n`
//     is biased (n rarely divides RAND_MAX+1) and mt19937 has a far better
//     period/quality.
//   - Optimal play is binary search: ceil(log2(range)) worst case (20/05).
//   - Stream recovery (in.clear()) is what a real reprompt loop needs (04).
// ============================================================
