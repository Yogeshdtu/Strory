// 08_flat_vs_pointer.cpp
// ============================================================
// Same algorithm, two layouts:
//   (A) pointer-based binary tree  -- each node a separate `new`
//   (B) flat array binary tree     -- node i's children at 2i+1, 2i+2
// Traverse both; the flat one is dramatically faster (cache).
// This is the folder's core lesson: layout beats Big-O.
// ============================================================
//   BENCH: g++ -std=c++20 -O2 08_flat_vs_pointer.cpp -o fp && ./fp
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---------- (A) pointer-based ----------
struct PNode {
    std::int64_t value;
    PNode* left  = nullptr;
    PNode* right = nullptr;
};

static PNode* buildPointer(int depth, std::mt19937_64& rng) {
    if (depth == 0) return nullptr;
    PNode* n = new PNode{static_cast<std::int64_t>(rng() & 0xffff), nullptr, nullptr};
    n->left  = buildPointer(depth - 1, rng);
    n->right = buildPointer(depth - 1, rng);
    return n;
}
static void destroyPointer(PNode* n) {
    if (!n) return;
    destroyPointer(n->left);
    destroyPointer(n->right);
    delete n;
}
static std::int64_t sumPointer(const PNode* n) {
    if (!n) return 0;
    return n->value + sumPointer(n->left) + sumPointer(n->right);
}

// ---------- (B) flat array ----------
// A perfect tree of `depth` levels has (2^depth - 1) nodes.
// node i: left child 2i+1, right child 2i+2. All contiguous.
static std::int64_t sumFlat(const std::vector<std::int64_t>& t, std::size_t i) {
    if (i >= t.size()) return 0;
    return t[i] + sumFlat(t, 2 * i + 1) + sumFlat(t, 2 * i + 2);
}
static std::int64_t sumFlatIter(const std::vector<std::int64_t>& t) {
    std::int64_t s = 0;
    for (std::int64_t v : t) s += v;                 // it's just a contiguous array -> linear scan
    return s;
}

int main() {
    const int DEPTH = 22;                            // 2^22 - 1 ~ 4.19M nodes
    const std::size_t N = (std::size_t{1} << DEPTH) - 1;

    std::mt19937_64 rng{2026};

    std::printf("=== building both (%d levels, %zu nodes) ===\n", DEPTH, N);

    // (A)
    std::mt19937_64 rngA = rng;
    PNode* root = buildPointer(DEPTH, rngA);         // N separate allocations, scattered

    // (B)
    std::mt19937_64 rngB = rng;
    std::vector<std::int64_t> flat(N);
    // node values are independent randoms -- we compare TRAVERSAL TIME, not the sum
    for (auto& v : flat) v = static_cast<std::int64_t>(rngB() & 0xffff);

    volatile std::int64_t sink = 0;

    std::printf("\n=== traverse: sum all node values (-O2) ===\n");

    auto t0 = Clock::now();
    sink += sumPointer(root);
    auto t1 = Clock::now();
    sink += sumFlat(flat, 0);
    auto t2 = Clock::now();
    sink += sumFlatIter(flat);
    auto t3 = Clock::now();

    auto ms = [](auto a, auto b){ return std::chrono::duration<double, std::milli>(b - a).count(); };
    double pTime = ms(t0, t1), fRec = ms(t1, t2), fIter = ms(t2, t3);

    std::printf("  pointer tree, recursive sum : %8.2f ms\n", pTime);
    std::printf("  flat array,   recursive sum : %8.2f ms   (%.1fx faster)\n", fRec,  pTime / fRec);
    std::printf("  flat array,   linear scan   : %8.2f ms   (%.1fx faster)\n", fIter, pTime / fIter);
    std::printf("  (sink %lld)\n", static_cast<long long>(sink));

    destroyPointer(root);

    std::printf(
        "\n"
        "  Identical work (visit N nodes, O(N)). The pointer tree pays ~1 cache miss per\n"
        "  node -- children are wherever the allocator put them. The flat array is one\n"
        "  contiguous block: the prefetcher streams it, and the pure linear scan also\n"
        "  auto-vectorizes. In HFT this is why trees/lists become arrays + index math.\n");
    return 0;
}
