// 07_no_exceptions.cpp
// ============================================================
// -fno-exceptions build ke liye error-handling toolkit. HFT/embedded
// mein exceptions aksar off hote hain — yeh file dono tarah compile
// hoti hai (exceptions on ya off) aur wahi kaam karti hai.
//
//   normal   :  g++ -std=c++20 -Wall -Wextra -Wshadow 07_no_exceptions.cpp -o ne && ./ne
//   no-exc   :  g++ -std=c++20 -fno-exceptions -Wall -Wextra 07_no_exceptions.cpp -o ne && ./ne
//
// -fno-exceptions ke saath jo BADAL jaata hai:
//   - throw  => compile error (isliye code mein koi throw nahi)
//   - new    => bad_alloc throw nahi kar sakta => std::terminate; new(nothrow) use karo
//   - vector::at(), stoi, etc. galat input pe terminate karte hain
//   - RAII, dtors, stack unwinding of NORMAL returns — sab waise hi
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>      // std::abort
#include <cstring>
#include <new>          // std::nothrow
#include <string_view>
#include <optional>

// ============================================================
//  1. [[nodiscard]] status enum — caller ko check karna PADEGA
// ============================================================
enum class [[nodiscard]] Status : std::uint8_t {
    ok = 0, overflow, bad_input, out_of_memory, not_found
};

static const char* to_str(Status s) {
    switch (s) {
        case Status::ok:            return "ok";
        case Status::overflow:      return "overflow";
        case Status::bad_input:     return "bad_input";
        case Status::out_of_memory: return "out_of_memory";
        case Status::not_found:     return "not_found";
    }
    return "?";
}

// ============================================================
//  2. FATAL macro — sach mein unrecoverable ke liye. exceptions ke
//     bina "stack ko wapas phenko" ka option nahi; seedha abort.
// ============================================================
#define FATAL(msg) do { \
    std::fprintf(stderr, "FATAL %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
    std::abort(); \
} while (0)

// ============================================================
//  3. Fixed-capacity container — kabhi allocate/throw nahi karta.
//     Overflow ko RETURN VALUE se report karta hai.
// ============================================================
template <class T, std::size_t Cap>
class FixedVec {
    T data_[Cap];
    std::size_t n_ = 0;
public:
    Status push_back(const T& v) {
        if (n_ == Cap) return Status::overflow;      // <-- no throw
        data_[n_++] = v;
        return Status::ok;
    }
    std::size_t size() const { return n_; }
    bool empty() const { return n_ == 0; }

    // checked access -> optional (no throw, no terminate)
    std::optional<T> get(std::size_t i) const {
        if (i >= n_) return std::nullopt;
        return data_[i];
    }
    // unchecked -> caller ki zimmedari (jaise operator[])
    const T& operator[](std::size_t i) const { return data_[i]; }
};

// ============================================================
//  4. Fallible parse — Status + out-param (koi throw nahi)
// ============================================================
static Status parse_u32(std::string_view s, std::uint32_t& out) {
    if (s.empty()) return Status::bad_input;
    std::uint64_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return Status::bad_input;
        n = n * 10 + static_cast<std::uint64_t>(c - '0');
        if (n > 0xFFFF'FFFFull) return Status::overflow;
    }
    out = static_cast<std::uint32_t>(n);
    return Status::ok;
}

// ============================================================
//  5. Heap chahiye to new(nothrow) — nullptr check, terminate nahi
// ============================================================
static Status make_buffer(std::size_t bytes, char*& out) {
    char* p = new (std::nothrow) char[bytes];    // fail => nullptr (throw nahi)
    if (!p) return Status::out_of_memory;
    std::memset(p, 0, bytes);
    out = p;
    return Status::ok;
}

int main() {
#if defined(__cpp_exceptions)
    std::puts("build: exceptions ON  (yeh file waise bhi throw use nahi karti)");
#else
    std::puts("build: -fno-exceptions (throw/try/catch is TU mein allowed hi nahi)");
#endif

    // --- FixedVec: overflow return se, terminate nahi ---
    std::puts("\n1. FixedVec<int,4> — 6 push, aakhri 2 overflow:");
    FixedVec<int, 4> fv;
    for (int i = 0; i < 6; ++i) {
        Status st = fv.push_back(i * 10);
        std::printf("   push(%d) -> %s\n", i * 10, to_str(st));
    }
    std::printf("   size=%zu\n", fv.size());

    // --- checked access ---
    std::puts("\n2. checked get():");
    for (std::size_t i : {std::size_t{0}, std::size_t{3}, std::size_t{9}}) {
        auto v = fv.get(i);
        if (v) std::printf("   get(%zu) = %d\n", i, *v);
        else   std::printf("   get(%zu) = <out of range>\n", i);
    }

    // --- parse ---
    std::puts("\n3. parse_u32:");
    const std::string_view ins[] = { "42", "", "12x", "4294967295", "9999999999" };
    for (std::string_view in : ins) {
        std::uint32_t out = 0;
        Status st = parse_u32(in, out);
        if (st == Status::ok) std::printf("   \"%.*s\" -> %u\n",
                                          static_cast<int>(in.size()), in.data(), out);
        else                  std::printf("   \"%.*s\" -> %s\n",
                                          static_cast<int>(in.size()), in.data(), to_str(st));
    }

    // --- nothrow heap ---
    std::puts("\n4. new(std::nothrow):");
    char* buf = nullptr;
    Status st = make_buffer(1024, buf);
    std::printf("   make_buffer(1024) -> %s\n", to_str(st));
    if (st == Status::ok) { std::strcpy(buf, "safe"); std::printf("   buf=\"%s\"\n", buf); }
    delete[] buf;

    // --- FATAL sirf sach-much-unrecoverable ke liye (yahan trigger nahi karte) ---
    std::puts("\n5. FATAL macro maujood hai un cheezon ke liye jahan aage badhna");
    std::puts("   hi galat ho (corrupt invariant). Yahan call nahi kar rahe.");
    if (fv.size() > 100) FATAL("impossible: size > capacity");

    std::puts("\nSaar (-fno-exceptions duniya):");
    std::puts("  - har fallible function Status/optional/expected lautaye, [[nodiscard]]");
    std::puts("  - heap: new(nothrow) + null-check, ya better: pehle se pool/arena");
    std::puts("  - truly unrecoverable: log + abort (FATAL)");
    std::puts("  - RAII/dtors bilkul waise hi kaam karte hain — sirf 'throw' gaya");
    return 0;
}
