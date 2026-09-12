# 18 — `<random>`: engines, distributions, seeding

## Prerequisites
- [`17-chrono.md`](17-chrono.md), folder 03 (integer ranges, modulo bias), folder 05 (bit ops)

## Yeh topic abhi kyun
Simulations, Monte Carlo pricing, randomized tests, market-data replay jitter —
sabko random numbers chahiye. `rand()` (C se) **bura** hai (low quality, small
range, biased `% n`, not thread-safe). `<random>` proper hai: **engine**
(bits banata) + **distribution** (unhe shape deta) — do alag cheezein.

---

## The two-part model

```cpp
#include <random>

std::mt19937 engine(seed);                    // ENGINE: produces uniformly-distributed raw bits
std::uniform_int_distribution<int> dist(1, 6);// DISTRIBUTION: maps engine output to a shape/range

int roll = dist(engine);                      // call the distribution WITH the engine
```

- **Engine** — a deterministic bit generator (a URBG: Uniform Random Bit
  Generator). Given the same seed, same sequence, every time, every platform.
- **Distribution** — stateless-ish transform: uniform int in `[a,b]`, uniform
  real, normal, exponential, poisson, ... It consumes bits from the engine as
  needed.

## Engines

| Engine | Notes |
|---|---|
| `std::mt19937` | Mersenne Twister, 32-bit. Good quality, **large state (~2.5 KB)**, fast enough. The common default. |
| `std::mt19937_64` | 64-bit variant. Use when you need 64-bit values. |
| `std::minstd_rand` | Linear congruential, tiny state, **lower quality** — ok for non-critical jitter. |
| `std::ranlux48` | High quality, **slow**. |
| `std::default_random_engine` | Implementation-defined alias (libstdc++: `minstd_rand`) — **avoid**, name the engine you want for reproducibility. |

`<random>` engines are **not cryptographically secure**. For security/tokens use
the OS CSPRNG (`getrandom`, `BCryptGenRandom`, `std::random_device` *if* it's
non-deterministic on your platform).

## Distributions

```cpp
std::uniform_int_distribution<int>       di(0, 100);       // [0, 100] inclusive both ends
std::uniform_real_distribution<double>   dr(0.0, 1.0);     // [0.0, 1.0)
std::bernoulli_distribution              coin(0.3);        // bool, P(true) = 0.3
std::normal_distribution<double>         gauss(0.0, 1.0);  // mean, stddev
std::exponential_distribution<double>    exp_(2.0);        // rate lambda -- inter-arrival times
std::poisson_distribution<int>           pois(4.0);        // events per interval
std::discrete_distribution<int>          weighted({10, 30, 60});  // index 0/1/2 with those weights

double x = gauss(engine);
```

Distributions may **hold state** between calls (e.g. `normal_distribution`
generates pairs and caches one). Reuse the distribution object; `.reset()` clears
cached state.

## Seeding — do it right

```cpp
// reproducible (tests, simulations you want to replay): a FIXED seed
std::mt19937 rng(42);

// non-reproducible (need a different run each time): seed from random_device,
// and seed the WHOLE state, not just 32 bits:
std::random_device rd;
std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
std::mt19937 rng(seq);

// per-thread: one engine per thread (engines are NOT thread-safe), seeded distinctly
thread_local std::mt19937 tls_rng(std::random_device{}() ^ hashOfThreadId());
```

- `std::random_device` — a *source* of non-deterministic bits **if the platform
  provides one** (it does on Windows and Linux). It's slow — use it only to seed,
  not per number. On some bare embedded libs it's a fixed PRNG (check
  `rd.entropy()`).
- Seeding `mt19937` with a single `int` leaves most of its 19937-bit state
  derived from a weak expansion → use a `seed_seq` of several `random_device`
  outputs for real unpredictability.
- **Log the seed.** A failing randomized test is only useful if you can replay it.

## Why `rand()` / `rand() % n` is bad

```cpp
int bad = rand() % 6 + 1;   // ❌
```
1. **Modulo bias** — if `RAND_MAX + 1` isn't a multiple of 6, low values are
   slightly more likely.
2. **Low quality** — many libc `rand()`s are a weak LCG; low bits are especially
   non-random (`rand() % 2` can alternate).
3. **Small range** — `RAND_MAX` is only guaranteed ≥ 32767.
4. **Global hidden state** — not thread-safe; `srand` in one place affects
   everywhere.

`std::uniform_int_distribution` handles the bias (rejection sampling) and uses
the engine's full range.

---

## Andar kya hota hai

- `mt19937` keeps 624 × 32-bit words of state; every 624 outputs it "twists" the
  whole array (a fixed sequence of shifts/xors/masks). O(1) amortized per number,
  but the big state blows a chunk of L1 and it's branchy — not the fastest.
- `uniform_int_distribution<int>(a, b)`: computes `range = b - a`, draws engine
  words, and **rejects** draws that would bias the result (values in the
  non-uniform tail) before `% (range+1)`. So it can call the engine more than
  once per number.
- `normal_distribution`: Box-Muller or ziggurat — generates two normals from a
  batch of uniforms and caches the second; hence the internal state and
  `.reset()`.
- Fast alternatives (not in the standard): `xoshiro256++`, `pcg32`, `wyrand` —
  tiny state (a few words), a handful of ops, pass statistical tests, ~1 ns/
  number. HFT sims that need billions of draws use these.

> **HFT relevance:** two modes. **Reproducible replay** — backtests and
> simulations seed a named engine (`mt19937_64`) with a **fixed, logged** seed so
> a run is bit-for-bit repeatable and a regression can be bisected.
> **Throughput** — Monte Carlo pricing / risk drawing billions of samples
> replaces `mt19937` (big state, branchy) with a small fast PRNG
> (`xoshiro`/`pcg`), one **`thread_local`** engine per worker (engines aren't
> thread-safe, and a shared one would false-share), and often a cheaper
> distribution (precomputed tables, direct bit tricks for `[0,1)` doubles). Never
> `rand()`. `std::random_device` only to seed, never in the loop.

---

## Hands-on

```cpp
// dice.cpp
#include <random>
#include <cstdio>
#include <map>

int main() {
    std::mt19937 rng(42);                     // fixed seed -> same output every run
    std::uniform_int_distribution<int> d6(1, 6);

    std::map<int,int> hist;
    for (int i = 0; i < 60000; ++i) ++hist[d6(rng)];
    for (auto [face, n] : hist) std::printf("%d: %d\n", face, n);   // ~10000 each

    std::normal_distribution<double> g(0.0, 1.0);
    double s = 0; for (int i = 0; i < 100000; ++i) s += g(rng);
    std::printf("normal mean ~ %.4f\n", s / 100000);                // ~0
}
```
```bash
g++ -std=c++20 -O2 dice.cpp -o dice && ./dice
```

---

## ⚠️ Traps

### Trap 1 — `rand() % n`
```cpp
int r = rand() % 100;   // ❌ modulo bias + weak low bits. std::uniform_int_distribution<int>(0, 99)(rng)
```

### Trap 2 — seeding with the current second
```cpp
std::mt19937 rng(time(nullptr));   // ⚠️ two processes started in the same second -> identical streams. Use random_device + seed_seq
```

### Trap 3 — one engine shared across threads
```cpp
static std::mt19937 rng;           // ⚠️ data race + correlated streams. thread_local engine per thread
```

### Trap 4 — recreating the engine every call
```cpp
int roll() { std::mt19937 e{std::random_device{}()}; return dist(e); }   // ⚠️ slow, and low-entropy if random_device is coarse. Keep the engine alive
```

### Trap 5 — `uniform_real_distribution(0,1)` and expecting `1.0` possible
```cpp
// range is [0.0, 1.0) -- 1.0 is never produced. Fine, just know it
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`rand()` is good enough" | Biased `% n`, weak low bits, tiny range, global state — use `<random>` |
| "The engine gives me the number I want" | Engine gives raw bits; a **distribution** shapes them into a range |
| "Seed `mt19937` with one `int`" | Weak state expansion — seed with a `seed_seq` of several `random_device` draws |
| "`std::random_device` is a fast RNG" | It's a slow entropy source — use it only to seed a PRNG |
| "`std::default_random_engine` is fine" | Implementation-defined — name a specific engine for reproducibility |

---

## Exercises

1. **Unbiased die:** produce a fair `1..6` with a named engine and correct
   seeding for a run you want to be *different* each time.

   <details><summary>Answer</summary>

   `std::random_device rd; std::seed_seq seq{rd(),rd(),rd(),rd()}; std::mt19937
   rng(seq); std::uniform_int_distribution<int> d6(1,6); int x = d6(rng);`
   </details>

2. **Reproducible test:** a randomized unit test failed on CI. What must the test
   have done for you to reproduce it locally?

   <details><summary>Answer</summary>

   Chosen its seed from an entropy source **and logged that seed** (e.g. printed
   `seed=123456789`). You then hard-code that seed locally to replay the exact
   sequence and debug.
   </details>

3. **Inter-arrival times:** simulate message arrivals at an average rate of 5000
   msgs/sec. Which distribution, what parameter, what does each draw give you?

   <details><summary>Answer</summary>

   `std::exponential_distribution<double> gap(5000.0);` — `gap(rng)` returns the
   time in **seconds** until the next message (mean `1/5000 = 200 µs`). Sum draws
   to get arrival timestamps (a Poisson process).
   </details>

4. **Thread pool:** 8 workers each need random numbers. Design the RNG setup.

   <details><summary>Answer</summary>

   `thread_local std::mt19937_64 rng(seed_for_this_thread);` where each thread's
   seed is distinct (e.g. a `seed_seq` mixing `random_device` with the worker
   index). No sharing → no races, no false sharing, independent streams.
   </details>

5. **Modulo bias:** `RAND_MAX == 32767`. You do `rand() % 3`. Which residue is
   over-represented and by how much (counts out of 32768 values)?

   <details><summary>Answer</summary>

   32768 = 3·10922 + 2, so residues 0 and 1 occur 10923 times, residue 2 occurs
   10922 times. Values 0 and 1 are ~0.009% more likely — small here, large when
   the modulus is close to `RAND_MAX`. `uniform_int_distribution` rejects the
   surplus to stay exact.
   </details>

---

## Interview questions

1. `<random>` ka two-part model — engine vs distribution?
2. `rand()` ke 4 problems?
3. `mt19937` ko sahi se seed kaise (single int kyun kaafi nahi)?
4. `std::random_device` kya hai — per-number use kyun nahi?
5. Multi-threaded code mein RNG — kaise (thread_local, kyun)?
6. Modulo bias kya, `uniform_int_distribution` use kaise handle karta?

---

## Next
→ [`19-filesystem.md`](19-filesystem.md)
