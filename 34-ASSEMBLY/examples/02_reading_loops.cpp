// 02_reading_loops.cpp
// ============================================================
// Loop patterns ko assembly mein pehchano. Har function ek alag loop
// shape hai. Compile karo aur asm dekho:
//
//   ./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp
//   g++ -std=c++20 -O2 -S -masm=intel 02_reading_loops.cpp -o - | c++filt
//
// Dekhne ki cheezein (Intel syntax):
//  - backward branch (`jne .L3`) = loop
//  - `add rax, 32` (not 4) in the body = vectorized 8-wide (int)
//  - `movdqu`/`paddd`/`ymm` = SIMD ; `add`/`mov [..]` scalar
//  - `test/jle` before the loop = entry guard (n <= 0 skip)
//  - `[rdi + rax*4]` = base + index*scale addressing
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_reading_loops.cpp -o loops && ./loops
// ============================================================

#include <cstdint>
#include <cstdio>
#include <vector>

// NOTE: yeh functions non-static hain -> compiler inhe standalone emit karta
// (external linkage), chahe main ke calls constant-fold ho jaayen. Isliye
// `./build.ps1 asm` mein har ek ki apni assembly dikhti hai. Koi `keep()`
// barrier nahi chahiye -- neeche printf har result ko "use" kar deta.

// 1. counted for-loop, simple reduction -> vectorizes (add rax, 32 in body)
std::int64_t sum_counted(const std::int32_t* a, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s += a[i];
    return s;
}

// 2. pointer-walk until sentinel -> unknown trip count, scalar, `while (*p)`
std::size_t strlen_like(const char* p) {
    const char* q = p;
    while (*q) ++q;
    return static_cast<std::size_t>(q - p);
}

// 3. do-while -> body runs at least once, one backward branch, no top guard
std::uint64_t collatz_steps(std::uint64_t n) {
    std::uint64_t steps = 0;
    do {
        n = (n & 1) ? (3 * n + 1) : (n >> 1);
        ++steps;
    } while (n != 1);
    return steps;
}

// 4. nested loop -> two backward branches, inner label inside outer
std::int64_t sum_matrix(const std::int32_t* m, std::size_t rows, std::size_t cols) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < rows; ++i)
        for (std::size_t j = 0; j < cols; ++j)
            s += m[i * cols + j];
    return s;
}

// 5. loop with early break -> a forward branch out of the loop body
std::size_t find_first(const std::int32_t* a, std::size_t n, std::int32_t target) {
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] == target) return i;
    return n;
}

// 6. loop with a data-dependent branch inside -> `if` becomes `cmov` at -O2
//    (folder 31/32: -O2 if-converts this; use #pragma to force a real jump)
std::int64_t sum_if_positive(const std::int32_t* a, std::size_t n) {
    std::int64_t s = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] > 0) s += a[i];
    return s;
}

int main() {
    std::vector<std::int32_t> v(1000);
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<std::int32_t>((i % 7) - 3);   // some negatives

    std::int64_t a = sum_counted(v.data(), v.size());
    std::size_t  b = strlen_like("assembly reading");
    std::uint64_t c = collatz_steps(27);
    std::int64_t d = sum_matrix(v.data(), 25, 40);
    std::size_t  e = find_first(v.data(), v.size(), 2);
    std::int64_t f = sum_if_positive(v.data(), v.size());

    std::printf("sum_counted     = %lld\n", static_cast<long long>(a));
    std::printf("strlen_like     = %zu\n", b);
    std::printf("collatz(27)     = %llu steps\n", static_cast<unsigned long long>(c));
    std::printf("sum_matrix      = %lld\n", static_cast<long long>(d));
    std::printf("find_first(2)   = %zu\n", e);
    std::printf("sum_if_positive = %lld\n", static_cast<long long>(f));

    std::puts("\nNow read the assembly:");
    std::puts("  ./build.ps1 asm 34-ASSEMBLY/examples/02_reading_loops.cpp");
    std::puts("  - sum_counted    : `paddd`/`add rax, 16or32` -> vectorized");
    std::puts("  - strlen_like    : scalar `cmp byte`, `jne` -> or a `call strlen`(!)");
    std::puts("  - collatz_steps  : `test`, `lea`/`shr`, one backward `jne`, no top guard");
    std::puts("  - sum_matrix     : outer + inner labels; inner likely vectorized");
    std::puts("  - find_first     : `cmp`, `je <return>` -> forward branch OUT of loop");
    std::puts("  - sum_if_positive: `cmovg` (if-converted) -- NOT a jump, at -O2");
    return 0;
}
