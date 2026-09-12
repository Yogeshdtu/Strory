// 09_cpp23_in_practice.cpp23.cpp
// ============================================================
// C++23 ke woh features jo roz ke code mein kaam aate hain -- sab is box
// (MinGW GCC 16.2) pe compile + run verified. Aakhri section mein
// std::flat_map vs std::map ka ASLI benchmark hai.
//
// Naam `.cpp23.cpp` hai: build.ps1 / Makefile aisi files ko apne aap
// -std=c++23 -lstdc++exp se compile karte hain.
// ============================================================
//   g++ -std=c++23 -Wall -Wextra -O2 09_cpp23_in_practice.cpp23.cpp -o cpp23 -lstdc++exp && ./cpp23
//
//   -lstdc++exp kyun? MinGW pe std::print ka Windows-terminal wala code us
//   library mein hai. Iske bina link error: undefined reference to
//   `std::__open_terminal` (is box pe measured).
//   Benchmark numbers ke liye -O2 zaroori hai -- -O0 pe timing bekaar hai.
// ============================================================

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <expected>
#include <flat_map>
#include <functional>
#include <generator>
#include <map>
#include <mdspan>
#include <memory>
#include <print>
#include <random>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// ============================================================
//  1. DEDUCING `this` -- ek function, teen overloads ki jagah
// ============================================================
// C++23 se pehle: `value()` ke teen version likhne padte the
//   T& value() &;   const T& value() const&;   T&& value() &&;
// Ab `this` ek normal parameter ki tarah likh sakte ho (`this Self&& self`),
// aur compiler khud deduce karta hai ki object lvalue hai, const hai, ya rvalue.
struct Holder {
    std::string data = "order-42";

    template <class Self>
    auto&& value(this Self&& self) {
        // `Self` batata hai object kaisa hai. std::forward se category aage jaati hai:
        // lvalue pe string& milega, rvalue (temporary) pe string&& (move ho sakta hai).
        return std::forward<Self>(self).data;
    }

    template <class Self>
    const char* kind(this Self&&) {
        // Sirf yeh dikhane ke liye ki compiler ne Self kya deduce kiya.
        if (std::is_rvalue_reference_v<Self&&>) return "rvalue (temporary)";
        if (std::is_const_v<std::remove_reference_t<Self>>) return "const lvalue";
        return "lvalue";
    }
};

// ============================================================
//  2. `static operator()` aur multi-dimensional `operator[]`
// ============================================================
// Stateless function object ka operator() ab `static` ho sakta hai --
// koi `this` pointer pass nahi hota (ek chhupa hua argument kam).
struct TickToPaise {
    static std::int64_t operator()(std::int64_t ticks) { return ticks * 5; }   // 1 tick = 5 paise
};

// `m[r, c]` -- C++23 se pehle comma yahan comma-operator ban jaata tha.
struct Grid {
    int cols;
    std::vector<int> cells;   // flat storage: row-major, cache ke liye achha
    int& operator[](int r, int c) { return cells[static_cast<std::size_t>(r * cols + c)]; }
};

// ============================================================
//  3. std::generator -- lazy sequence, bina hand-rolled coroutine plumbing ke
// ============================================================
struct Tick { std::uint64_t seq; std::int64_t px; };

// Market-data replay jaisa: har `co_yield` pe ek tick deta hai, phir ruk jaata hai.
// Caller jitne maange utne hi bante hain -- poora vector memory mein nahi.
std::generator<Tick> replay(std::int64_t start_px) {
    std::int64_t px = start_px;
    for (std::uint64_t seq = 1;; ++seq) {          // infinite -- caller take() se rokega
        px += (seq % 3 == 0) ? -1 : 1;
        co_yield Tick{seq, px};
    }
}

// ============================================================
//  5. std::expected -- monadic chaining (poori detail: 23-ERROR-HANDLING/10)
// ============================================================
enum class Err { empty, bad_qty };

std::expected<int, Err> parse_qty(const std::string& s) {
    if (s.empty()) return std::unexpected(Err::empty);
    int q = 0;
    for (char ch : s) {
        if (ch < '0' || ch > '9') return std::unexpected(Err::bad_qty);
        q = q * 10 + (ch - '0');
    }
    return q;
}

// ============================================================
//  7. chhote utilities -- std::to_underlying, std::byteswap, std::unreachable
// ============================================================
enum class Side : std::uint8_t { Buy = 'B', Sell = 'S' };

int side_sign(Side s) {
    switch (s) {
        case Side::Buy:  return +1;
        case Side::Sell: return -1;
    }
    // Enum ke saare cases upar handle ho gaye. std::unreachable() compiler ko
    // batata hai "yahan kabhi nahi aayenge" -- galat nikla to UB. Sirf tab likho
    // jab input pehle validate ho chuka ho.
    std::unreachable();
}

// if consteval: compile-time pe ek raasta, runtime pe doosra.
constexpr int where_am_i() {
    if consteval { return 1; }   // compile time
    else         { return 2; }   // runtime
}

// ============================================================
//  8. flat_map vs map -- benchmark helpers
// ============================================================
using Clock = std::chrono::steady_clock;

template <class Fn>
double best_ns_per_op(Fn&& fn, std::size_t ops, int repeats = 5) {
    // Kai baar chalao, sabse tez run lo -- OS noise (context switch, interrupts)
    // sirf slow direction mein jodta hai, isliye minimum sabse saaf signal hai.
    double best = 1e18;
    for (int r = 0; r < repeats; ++r) {
        const auto t0 = Clock::now();
        fn();
        const auto t1 = Clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(ops);
        best = std::min(best, ns);
    }
    return best;
}

void bench_lookup(std::size_t n_keys) {
    std::mt19937_64 rng(42);
    std::vector<std::int64_t> keys(n_keys);
    for (auto& k : keys) k = static_cast<std::int64_t>(rng() % (n_keys * 4));

    std::map<std::int64_t, std::int64_t> tree;
    std::flat_map<std::int64_t, std::int64_t> flat;
    for (auto k : keys) { tree[k] = k; flat[k] = k; }

    // Lookup ke keys PEHLE se bana lo -- warna RNG ki cost bhi time mein aa jaati
    // (Rule 2: sirf wahi measure karo jo claim kar rahe ho).
    const std::size_t n_lookups = 2'000'000;
    std::vector<std::int64_t> probes(n_lookups);
    for (auto& p : probes) p = static_cast<std::int64_t>(rng() % (n_keys * 4));

    std::int64_t sink_tree = 0, sink_flat = 0;
    const double t_tree = best_ns_per_op([&] {
        std::int64_t s = 0;
        for (auto p : probes) { auto it = tree.find(p); if (it != tree.end()) s += it->second; }
        sink_tree = s;
    }, n_lookups);
    const double t_flat = best_ns_per_op([&] {
        std::int64_t s = 0;
        for (auto p : probes) { auto it = flat.find(p); if (it != flat.end()) s += it->second; }
        sink_flat = s;
    }, n_lookups);

    std::println("  lookup  keys={:>7}   map {:6.1f} ns   flat_map {:6.1f} ns   ratio {:4.2f}x   (checksum {})",
                 tree.size(), t_tree, t_flat, t_tree / t_flat, sink_tree == sink_flat ? "match" : "MISMATCH");
}

void bench_insert(std::size_t n_keys) {
    std::mt19937_64 rng(7);
    std::vector<std::int64_t> keys(n_keys);
    for (auto& k : keys) k = static_cast<std::int64_t>(rng());

    // Har repeat mein naya container -- warna doosri baar keys pehle se hongi.
    const double t_tree = best_ns_per_op([&] {
        std::map<std::int64_t, std::int64_t> m;
        for (auto k : keys) m.emplace(k, k);
    }, n_keys, 3);
    const double t_flat = best_ns_per_op([&] {
        std::flat_map<std::int64_t, std::int64_t> m;
        for (auto k : keys) m.emplace(k, k);   // random order -> har insert beech mein shift karta hai
    }, n_keys, 3);

    std::println("  insert  keys={:>7}   map {:6.1f} ns   flat_map {:6.1f} ns   ratio {:4.2f}x  (random order)",
                 n_keys, t_tree, t_flat, t_flat / t_tree);
}

int main() {
    // ============================================================
    //  1. DEDUCING `this`
    // ============================================================
    std::println("== 1. deducing this ==");
    Holder h;
    const Holder ch;
    std::println("  h.kind()        -> {}", h.kind());
    std::println("  ch.kind()       -> {}", ch.kind());
    std::println("  Holder{{}}.kind() -> {}", Holder{}.kind());
    std::string moved = Holder{}.value();          // rvalue se string MOVE hui, copy nahi
    std::println("  value() from temporary moved out: \"{}\"", moved);

    // Recursive lambda: pehle std::function ya Y-combinator chahiye tha.
    // `this auto self` se lambda khud ko call kar sakta hai -- zero overhead.
    auto fib = [](this auto self, int n) -> long long { return n < 2 ? n : self(n - 1) + self(n - 2); };
    std::println("  recursive lambda fib(30) = {}", fib(30));

    // ============================================================
    //  2. static operator() + m[r, c]
    // ============================================================
    std::println("\n== 2. static operator() + multidimensional [] ==");
    std::println("  TickToPaise{{}}(7) = {} paise", TickToPaise{}(7));
    Grid g{3, std::vector<int>(6, 0)};
    g[1, 2] = 99;
    std::println("  g[1, 2] = {}   (flat index {})", g[1, 2], 1 * 3 + 2);

    // std::mdspan: wahi "flat buffer + stride" idea, standard library mein.
    // Yeh NON-OWNING view hai -- memory g.cells ki hai, mdspan sirf pointer + extents rakhta hai.
    std::mdspan view(g.cells.data(), 2, 3);          // 2 rows, 3 cols
    view[0, 1] = 7;                                  // view se likha -> g.cells[1] badal gaya
    std::println("  mdspan view[1, 2] = {}   g.cells[1] = {}   extents {}x{}   sizeof(view) = {} bytes",
                 view[1, 2], g.cells[1], view.extent(0), view.extent(1), sizeof(view));

    // ============================================================
    //  3. std::generator
    // ============================================================
    std::println("\n== 3. std::generator (lazy tick replay) ==");
    for (const Tick& t : replay(10'000) | std::views::take(5)) {
        std::println("  seq={} px={}", t.seq, t.px);
    }

    // ============================================================
    //  4. RANGES ADDITIONS
    // ============================================================
    std::println("\n== 4. ranges: enumerate / zip / pairwise / chunk / to / fold_left ==");
    std::vector<std::int64_t> bid_px{100, 99, 97};
    std::vector<std::int64_t> bid_qty{5, 12, 40};

    for (auto [level, px] : std::views::enumerate(bid_px))          // index + value, bina manual counter
        std::println("  level {} px {}", level, px);

    // zip: do vectors ek saath -- "parallel arrays" (SoA) ke liye perfect.
    auto notional = std::views::zip(bid_px, bid_qty)
                  | std::views::transform([](auto pq) { auto [p, q] = pq; return p * q; })
                  | std::ranges::to<std::vector>();                   // pipeline -> seedha vector
    std::println("  notional per level: {}", notional);

    // pairwise: lagataar do-do elements -- price gaps nikalne ke liye.
    for (auto [a, b] : bid_px | std::views::pairwise)
        std::println("  gap {} -> {} = {}", a, b, a - b);

    // chunk: 2-2 ke batches (batching, folder 36/16).
    for (auto batch : std::views::iota(1, 8) | std::views::chunk(3))
        std::println("  batch size {}", std::ranges::distance(batch));

    // fold_left: std::accumulate ka ranges version.
    std::println("  total qty = {}", std::ranges::fold_left(bid_qty, std::int64_t{0}, std::plus{}));

    // ============================================================
    //  5. std::expected monadic
    // ============================================================
    std::println("\n== 5. std::expected: and_then / transform / or_else ==");
    for (std::string input : {"250", "", "2x0"}) {
        auto lots = parse_qty(input)
                        .and_then([](int q) -> std::expected<int, Err> {
                            if (q == 0) return std::unexpected(Err::bad_qty);
                            return q;
                        })
                        .transform([](int q) { return q / 50; })      // 50 shares = 1 lot
                        .or_else([](Err e) -> std::expected<int, Err> {
                            return std::unexpected(e);                // yahan log / map kar sakte the
                        });
        if (lots) std::println("  \"{}\" -> {} lots", input, *lots);
        else      std::println("  \"{}\" -> error {}", input, std::to_underlying(lots.error()));
    }

    // ============================================================
    //  6. std::move_only_function
    // ============================================================
    std::println("\n== 6. std::move_only_function ==");
    auto order = std::make_unique<std::int64_t>(123);
    // std::function<std::int64_t()> bad = [o = std::move(order)] { return *o; };
    //   ^ COMPILE ERROR: std::function ko copyable callable chahiye, unique_ptr copy nahi hota.
    std::move_only_function<std::int64_t()> task = [o = std::move(order)] { return *o; };
    std::println("  task() = {}   (lambda ke andar unique_ptr hai)", task());

    // ============================================================
    //  7. chhote utilities
    // ============================================================
    std::println("\n== 7. small utilities ==");
    std::println("  to_underlying(Side::Sell) = '{}'", static_cast<char>(std::to_underlying(Side::Sell)));
    std::println("  side_sign(Buy) = {}", side_sign(Side::Buy));
    // Wire protocols (ITCH etc.) numbers big-endian bhejte hain; x86 little-endian hai.
    // Network se 4 bytes aaye: 00 00 27 10  (= 10000 big-endian mein).
    const unsigned char wire_bytes[4] = {0x00, 0x00, 0x27, 0x10};
    std::uint32_t raw = 0;
    std::memcpy(&raw, wire_bytes, sizeof raw);                        // x86 pe galat padha jaata hai
    std::println("  wire bytes read as-is = {}   byteswap -> {}", raw, std::byteswap(raw));
    std::string sym = "NSE:RELIANCE";
    std::println("  \"{}\".contains(\"NSE:\") = {}", sym, sym.contains("NSE:"));
    std::vector<int> original{1, 2};
    auto copy = auto(original);                                       // auto(x): saaf "decay copy"
    copy.push_back(3);
    std::println("  auto(x) copy size {} vs original {}", copy.size(), original.size());
    constexpr int ct = where_am_i();
    int rt = where_am_i();
    std::println("  if consteval: compile-time {} / runtime {}", ct, rt);

    // ============================================================
    //  8. flat_map vs map -- ASLI benchmark (-O2 pe chalao)
    // ============================================================
    std::println("\n== 8. flat_map vs map (best of 5, -O2 required) ==");
    bench_lookup(64);          // ek chhota price-level map jaisa
    bench_lookup(4'096);
    bench_lookup(262'144);
    bench_insert(1'000);
    bench_insert(50'000);
}
