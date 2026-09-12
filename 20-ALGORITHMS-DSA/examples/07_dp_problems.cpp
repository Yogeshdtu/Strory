// 07_dp_problems.cpp
// ============================================================
// Dynamic programming -- the three classics, each shown as
// (a) naive recursion, (b) memoized (top-down), (c) tabulated (bottom-up),
// plus a space-optimized version where it applies.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_dp_problems.cpp -o dp && ./dp
//   BENCH: g++ -std=c++20 -O2 07_dp_problems.cpp -o dp && ./dp
// ============================================================
//   1. Fibonacci        -- overlapping subproblems, O(2^n) -> O(n)
//   2. 0/1 Knapsack     -- 2D table, then rolling 1D row
//   3. Longest Common Subsequence -- 2D table + reconstruction
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---------- 1. Fibonacci ----------
static long long fibNaive(int n) {
    if (n < 2) return n;
    return fibNaive(n - 1) + fibNaive(n - 2);              // recomputes the same subtrees
}
static long long fibMemo(int n, std::vector<long long>& memo) {
    if (n < 2) return n;
    long long& slot = memo[static_cast<std::size_t>(n)];
    if (slot != -1) return slot;
    return slot = fibMemo(n - 1, memo) + fibMemo(n - 2, memo);
}
static long long fibTab(int n) {
    if (n < 2) return n;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; ++i) { long long c = a + b; a = b; b = c; }   // O(1) space
    return b;
}

// ---------- 2. 0/1 Knapsack ----------
static int knapsack2D(const std::vector<int>& wt, const std::vector<int>& val, int cap) {
    const std::size_t n = wt.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(static_cast<std::size_t>(cap) + 1, 0));
    for (std::size_t i = 1; i <= n; ++i)
        for (int c = 0; c <= cap; ++c) {
            int wi = wt[i - 1], vi = val[i - 1];
            dp[i][static_cast<std::size_t>(c)] = dp[i - 1][static_cast<std::size_t>(c)];
            if (wi <= c)
                dp[i][static_cast<std::size_t>(c)] =
                    std::max(dp[i][static_cast<std::size_t>(c)],
                             dp[i - 1][static_cast<std::size_t>(c - wi)] + vi);
        }
    return dp[n][static_cast<std::size_t>(cap)];
}
static int knapsack1D(const std::vector<int>& wt, const std::vector<int>& val, int cap) {
    std::vector<int> dp(static_cast<std::size_t>(cap) + 1, 0);
    for (std::size_t i = 0; i < wt.size(); ++i)
        for (int c = cap; c >= wt[i]; --c)                 // iterate c DOWNWARD -> each item used once
            dp[static_cast<std::size_t>(c)] =
                std::max(dp[static_cast<std::size_t>(c)],
                         dp[static_cast<std::size_t>(c - wt[i])] + val[i]);
    return dp[static_cast<std::size_t>(cap)];
}

// ---------- 3. Longest Common Subsequence ----------
static std::string lcs(const std::string& a, const std::string& b) {
    const std::size_t n = a.size(), m = b.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (std::size_t i = 1; i <= n; ++i)
        for (std::size_t j = 1; j <= m; ++j)
            dp[i][j] = (a[i - 1] == b[j - 1]) ? dp[i - 1][j - 1] + 1
                                              : std::max(dp[i - 1][j], dp[i][j - 1]);
    // reconstruct
    std::string out;
    for (std::size_t i = n, j = m; i > 0 && j > 0; ) {
        if (a[i - 1] == b[j - 1]) { out.push_back(a[i - 1]); --i; --j; }
        else if (dp[i - 1][j] >= dp[i][j - 1]) --i;
        else --j;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

int main() {
    std::printf("=== 1. Fibonacci ===\n");
    {
        int n = 40;
        auto t0 = Clock::now();
        long long r1 = fibNaive(n);
        auto t1 = Clock::now();
        std::vector<long long> memo(static_cast<std::size_t>(n) + 1, -1);
        long long r2 = fibMemo(n, memo);
        auto t2 = Clock::now();
        long long r3 = fibTab(n);
        auto t3 = Clock::now();
        auto ms = [](auto a, auto b){ return std::chrono::duration<double, std::milli>(b - a).count(); };
        std::printf("  fib(%d) = %lld\n", n, r1);
        std::printf("  naive  O(2^n) : %8.2f ms   (r=%lld)\n", ms(t0, t1), r1);
        std::printf("  memo   O(n)   : %8.4f ms   (r=%lld)\n", ms(t1, t2), r2);
        std::printf("  tab    O(n)   : %8.4f ms   (r=%lld)\n", ms(t2, t3), r3);
        std::printf("  speedup naive->memo: ~%.0fx\n", ms(t0, t1) / (ms(t1, t2) + 1e-9));
    }

    std::printf("\n=== 2. 0/1 Knapsack ===\n");
    {
        std::vector<int> wt {2, 3, 4, 5, 9};
        std::vector<int> val{3, 4, 5, 8, 10};
        int cap = 20;
        std::printf("  items (w,v): (2,3)(3,4)(4,5)(5,8)(9,10)   capacity %d\n", cap);
        std::printf("  best value  2D table : %d\n", knapsack2D(wt, val, cap));
        std::printf("  best value  1D rolling: %d   (O(n*cap) time, O(cap) space)\n", knapsack1D(wt, val, cap));
    }

    std::printf("\n=== 3. Longest Common Subsequence ===\n");
    {
        std::string a = "AGGTABCDEF";
        std::string b = "GXTXAYBCF";
        std::string r = lcs(a, b);
        std::printf("  a = %s\n  b = %s\n", a.c_str(), b.c_str());
        std::printf("  LCS = \"%s\"  (length %zu)\n", r.c_str(), r.size());
    }

    std::printf(
        "\n"
        "  DP recipe: (1) state = what fully describes a subproblem, (2) transition =\n"
        "  how states combine, (3) base cases, (4) order (memoize top-down, or tabulate\n"
        "  bottom-up), (5) optionally shrink the table when a row only needs the previous.\n");
    return 0;
}
