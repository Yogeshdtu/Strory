# 16 — String algorithms: KMP, rolling hash, Z

## Prerequisites
- Folder 10 (strings, `string_view`), [`08-hash-tables.md`](08-hash-tables.md) (hashing)
- [`01-complexity-analysis.md`](01-complexity-analysis.md)

## Yeh topic abhi kyun
"Kya `pattern` `text` mein hai, aur kahan?" — naive answer `O(n·m)` hai. Teen
classic techniques ise `O(n + m)` bana dete: **KMP** (prefix function),
**rolling hash / Rabin-Karp**, aur **Z-algorithm**. Log parsing, protocol
framing, aur dedup ke liye relevant.

---

## The naive baseline — `O(n·m)`

```cpp
// find first occurrence of pat in txt, brute force
std::size_t naiveFind(std::string_view txt, std::string_view pat) {
    if (pat.empty()) return 0;
    for (std::size_t i = 0; i + pat.size() <= txt.size(); ++i) {
        std::size_t j = 0;
        while (j < pat.size() && txt[i + j] == pat[j]) ++j;
        if (j == pat.size()) return i;
    }
    return std::string_view::npos;
}
```

Worst case (`txt = "aaaa…a"`, `pat = "aaa…ab"`) → `O(n·m)`: every alignment
matches `m-1` chars then fails. In practice `std::string::find` is this plus
SIMD/`memchr` tricks and is usually fine — the special algorithms win on
adversarial inputs or when you need *all* occurrences / repeated searches.

---

## KMP — Knuth-Morris-Pratt, `O(n + m)`

**Idea:** when a mismatch happens after matching `j` characters, you already know
those `j` characters — don't re-check them. The **prefix function** `π[i]` = the
length of the longest proper prefix of `pat[0..i]` that is also a suffix of
`pat[0..i]`. It tells you how far to "fall back" the pattern pointer on a
mismatch without moving the text pointer.

```cpp
std::vector<int> prefixFunction(std::string_view s) {
    std::vector<int> pi(s.size(), 0);
    for (std::size_t i = 1; i < s.size(); ++i) {
        int j = pi[i - 1];
        while (j > 0 && s[i] != s[static_cast<std::size_t>(j)]) j = pi[static_cast<std::size_t>(j - 1)];
        if (s[i] == s[static_cast<std::size_t>(j)]) ++j;
        pi[i] = j;
    }
    return pi;
}

std::size_t kmpFind(std::string_view txt, std::string_view pat) {
    if (pat.empty()) return 0;
    auto pi = prefixFunction(pat);
    int j = 0;
    for (std::size_t i = 0; i < txt.size(); ++i) {
        while (j > 0 && txt[i] != pat[static_cast<std::size_t>(j)]) j = pi[static_cast<std::size_t>(j - 1)];
        if (txt[i] == pat[static_cast<std::size_t>(j)]) ++j;
        if (j == static_cast<int>(pat.size())) return i - pat.size() + 1;   // match; j = pi[j-1] to keep going
    }
    return std::string_view::npos;
}
```

- Prefix function: `O(m)` (the `while` total is amortized — `j` increases by ≤ 1
  per step and never goes below 0).
- Search: `O(n)` — the text pointer `i` never moves backward.
- Uses: `pat` in `txt`, count/locate all occurrences, `string` periodicity
  (`n - π[n-1]` is the smallest period), building the Aho-Corasick automaton
  (multi-pattern KMP).

---

## Rolling hash / Rabin-Karp — `O(n + m)` expected

Hash the pattern once. Slide a window over the text, maintaining its hash in
`O(1)` per step (add the new char, remove the old — a "rolling" polynomial hash).
Compare hashes; on a hit, **verify** with a real char comparison (hashes can
collide).

```cpp
// polynomial hash: h(s) = s[0]*B^(m-1) + s[1]*B^(m-2) + ... + s[m-1]   (mod 2^64, implicit)
std::size_t rkFind(std::string_view txt, std::string_view pat) {
    const std::size_t m = pat.size(), n = txt.size();
    if (m == 0) return 0;
    if (m > n) return std::string_view::npos;
    const std::uint64_t B = 1315423911u;
    std::uint64_t hp = 0, ht = 0, pow = 1;
    for (std::size_t i = 0; i < m; ++i) {
        hp = hp * B + static_cast<std::uint8_t>(pat[i]);
        ht = ht * B + static_cast<std::uint8_t>(txt[i]);
        if (i + 1 < m) pow *= B;                      // pow = B^(m-1)
    }
    for (std::size_t i = 0; ; ++i) {
        if (hp == ht && txt.substr(i, m) == pat) return i;     // verify!
        if (i + m >= n) break;
        ht = (ht - static_cast<std::uint8_t>(txt[i]) * pow) * B + static_cast<std::uint8_t>(txt[i + m]);
    }
    return std::string_view::npos;
}
```

- **Best when searching for many patterns of the same length** (hash all, one
  pass) or when you need substring **equality checks** all over an algorithm
  (suffix comparisons, LCP, palindrome checks) — precompute prefix hashes, then
  any substring hash is `O(1)`.
- **Collisions**: mod `2^64` (implicit) is fast but attackable; use a random base
  / double hashing (two different mods) for safety, and always verify a hit.
- Expected `O(n + m)`; worst case `O(n·m)` if every window collides (rare with a
  good random base).

---

## Z-algorithm — `O(n)`

`z[i]` = length of the longest substring starting at `i` that matches a **prefix**
of the string. Computed in one pass using a `[l, r]` "Z-box" of the rightmost
match seen.

For pattern matching: build the string `pat + '\0' + txt`, compute `z`; any `i`
in the `txt` part with `z[i] >= m` is a match at `i - m - 1`.

Z and the prefix function are interconvertible; use whichever is cleaner for the
problem. Z is nice for "longest common prefix of `s` and each suffix of `s`",
string compression, and some palindrome problems.

---

## Other string tools (map)

| Need | Tool |
|---|---|
| single pattern, one text | `std::string::find` (naive + SIMD) — usually enough; KMP for adversarial |
| all occurrences / periodicity | KMP prefix function |
| **many** patterns, one pass | **Aho-Corasick** (trie + KMP-style fail links) |
| many substring-equality queries in one algorithm | **prefix rolling hashes** (`O(1)` per query) |
| all substrings of one text, repeated pattern queries | **suffix array + LCP** (`O(n log n)` build, `O(m log n)` query) or suffix automaton |
| longest common substring / repeated substring | suffix array/automaton, or binary search + rolling hash |
| edit distance / alignment | DP (file 14) |
| approximate / fuzzy | DP with a band, or `bitap`, or a specialized library |

`std::regex` is **not** on this list for anything performance-sensitive (folder
19 file 20 — µs construction, backtracking).

---

## Andar kya hota hai

- **KMP** is `O(n + m)` with a **tiny constant** and near-perfect branch
  prediction on realistic text — the text pointer only moves forward, the `while`
  fallback is amortized. It's the algorithm behind `std::search` when you pass a
  `std::boyer_moore_searcher` alternative (folder 19 file 09), and behind many
  `grep`-like tools.
- **Rolling hash** does ~3 arithmetic ops per character (mul, add, sub) — no
  branches in the slide → vectorizable, streaming. The rare verify is a `memcmp`.
  This is why it's the tool for "compare lots of substrings" inside a bigger
  algorithm.
- Naive search with `memchr` to find the first character of `pat`, then a
  `memcmp` for the rest, is often **faster than KMP in practice** for short
  patterns in non-adversarial text — SIMD `memchr` skips huge spans. KMP wins
  when the alphabet is tiny and the text has lots of partial matches.
- All three operate on **contiguous** `char` data — cache-friendly linear scans.
  Use `std::string_view` to avoid copies (folder 10).

> **HFT relevance:** exact string search shows up in **log/journal scanning**
> (offline), **protocol framing** (finding a delimiter or sync word in a byte
> stream — usually `memchr` / a fixed-offset check, not KMP), and **symbol
> normalization**. Rolling hashes are used for **deduplication / content
> fingerprinting** of messages and for fast substring-equality inside parsers.
> The hot-path reality is that market-data parsing is **fixed-layout** (known
> field offsets, `std::from_chars` for numbers) — no search at all — and any
> genuine string search is `memchr`-based. KMP / Z / suffix structures are
> interview and offline-tooling material; know them, but the tick path rarely
> needs them.

---

## Hands-on

```bash
# no dedicated example -- build one:
```
```cpp
// strfind.cpp
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
// paste prefixFunction + kmpFind from the lesson
int main() {
    std::string txt = "abxabcabcaby";
    std::string pat = "abcaby";
    std::printf("naive %zu\n", txt.find(pat));
    std::printf("kmp   %zu\n", kmpFind(txt, pat));
    auto pi = prefixFunction("aabaaab");
    std::printf("pi(aabaaab): "); for (int x : pi) std::printf("%d ", x); std::printf("\n");
}
```
```bash
g++ -std=c++20 -O2 -Wall strfind.cpp -o sf && ./sf
```

Implement KMP and rolling-hash search; test both against `std::string::find` on
random strings and on the adversarial `"aa...a" / "aa...ab"` case. Add "count all
occurrences" and "smallest period of a string" using the prefix function.

---

## ⚠️ Traps

### Trap 1 — rolling hash without verification
```cpp
if (hp == ht) return i;   // ⚠️ hash collision -> false match. Always compare the actual substring on a hit.
```

### Trap 2 — KMP prefix function off-by-one
```cpp
// pi[i] uses pi[i-1] as the fallback start; the while compares s[i] against s[j], j = pi[j-1] on mismatch.
// Getting the index (j vs j-1) wrong silently breaks matching.
```

### Trap 3 — building `pat + txt` without a separator for Z-based matching
```cpp
// Use a sentinel not in the alphabet (pat + '\0' + txt) so a "prefix match" can't span the boundary.
```

### Trap 4 — weak / non-random rolling-hash base
```cpp
// A fixed small base + mod 2^64 is attackable (anti-hash tests). Random base + double hashing for safety.
```

### Trap 5 — reaching for KMP when `std::string::find` is fine
```cpp
// For short patterns in normal text, SIMD memchr + memcmp beats KMP. Profile before switching.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Naive search is `O(n + m)`" | `O(n·m)` worst case (many partial matches) |
| "Rolling-hash match = confirmed match" | Hashes collide — verify the substring |
| "KMP is always faster than `std::string::find`" | For short patterns in normal text, SIMD naive often wins |
| "The prefix function is `O(m²)`" | `O(m)` amortized — `j` rises by ≤ 1 per step |
| "Use `std::regex` for substring search" | Slow (folder 19 file 20); use `find` / KMP / rolling hash |

---

## Exercises

1. **Prefix function:** compute `π` for `"ababcabab"`. What is the string's
   smallest period?

   <details><summary>Answer</summary>

   `π = [0,0,1,2,0,1,2,3,4]`. Smallest period = `n - π[n-1] = 9 - 4 = 5`
   (`"ababc"` repeated, then a partial). It's a full period only if `n % period
   == 0` (here 9 % 5 ≠ 0, so it's periodic-with-remainder, not fully periodic).
   </details>

2. **All occurrences:** modify `kmpFind` to return every start index of `pat` in
   `txt`.

   <details><summary>Answer</summary>

   When `j == m`, record `i - m + 1`, then set `j = pi[m - 1]` (instead of
   returning) and continue the loop. `O(n + m)` total.
   </details>

3. **Substring hash:** given prefix hashes `H[i]` of a string and `pow[k] = B^k`,
   give the `O(1)` formula for the hash of `s[l..r]`.

   <details><summary>Answer</summary>

   With `H[i]` = hash of `s[0..i)` and `H[0] = 0`: `hash(s[l..r]) = H[r+1] - H[l]
   * pow[r + 1 - l]` (all mod the chosen modulus). Compare two substrings'
   hashes in `O(1)`.
   </details>

4. **Rabin-Karp for many patterns:** you have 10,000 forbidden words all of
   length 8. Efficient scan of a large text?

   <details><summary>Answer</summary>

   Hash all 10,000 words into a `std::unordered_set<uint64_t>`. Slide one
   length-8 rolling-hash window over the text; on each step check membership in
   the set (`O(1)`), verify on a hit. `O(n)` expected. (Aho-Corasick handles
   mixed lengths in one pass.)
   </details>

5. **Why verify:** with hashes mod `2^64` and a random base, roughly what's the
   false-positive probability per comparison, and why still verify?

   <details><summary>Answer</summary>

   ~`2⁻⁶⁴` per comparison — astronomically small in theory. But (a) mod `2^64`
   without a prime is weaker than it looks and attackable, (b) over `n` windows
   the union bound is `n · 2⁻⁶⁴` — still tiny, but a single false match returns a
   wrong index. Verification is `O(m)` on the rare hit and removes all doubt.
   </details>

---

## Interview questions

1. Naive search ka worst case `O(n·m)` — kaunsa input?
2. KMP ka prefix function kya batata, `O(m)` kyun (amortized)?
3. Rolling hash `O(1)` per slide kaise, verify kyun zaroori?
4. Z-algorithm kya compute karta, pattern matching mein kaise use?
5. Many patterns one pass — kaunsa algorithm (Aho-Corasick)?
6. Market-data parsing mein string search kyun kam, kya use hota (fixed offsets, `memchr`)?

---

## Next
→ [`17-cache-aware-dsa.md`](17-cache-aware-dsa.md)
