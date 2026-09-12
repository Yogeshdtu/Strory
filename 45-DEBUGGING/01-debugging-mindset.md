# 01 — Debugging mindset: bug ek science problem hai, guess nahi

## Prerequisites
- `02-CPP-FIRST-STEPS/10-your-first-errors.md` (compiler errors padhna)
- `23-ERROR-HANDLING/13-undefined-behaviour.md` (UB kya hota hai)
- Koi bhi ek folder jisme tumne khud code likha aur woh galat chala

## Yeh topic abhi kyun

Ab tak tumne 44 folders ka code padha aur likha. Har folder mein ek
`## ⚠️ Traps` section tha, har example ke neeche ek bug ka demo. Tumne
**bugs dekhe**. Ab tumhe **bug dhoondhna** seekhna hai — systematically,
har baar, chahe bug tumhara ho ya kisi aur ka, chahe 10-line program ho ya
50,000-line trading system.

Debugging ek alag skill hai. Achhe programmers bhi kharaab debuggers hote
hain agar unhone kabhi process seekha hi nahi — woh bas code ko ghoorte
hain aur "shayad yeh?" try karte hain. Yeh folder woh process deta hai.

Yeh folder ek **skill folder** hai (jaise `20-DSA` ya `35-PROFILING`) —
ise ek baar padho, phir jab bhi koi bug aaye, wapas aao aur relevant lesson
kholo.

---

## Debugging = hypothesis → test → repeat

Ek bug fix karna **exactly** vaisa hi hai jaisa science mein ek theory
test karna:

```
   1. OBSERVE      program X input pe Y karta hai; hona chahiye tha Z
        │
        ▼
   2. HYPOTHESIZE  "shayad `count` loop ke baad off-by-one hai"
        │             (ek specific, testable claim — mechanism ke saath)
        ▼
   3. PREDICT      "agar yeh sach hai, to `count` yahan 8 hoga, 7 nahi"
        │
        ▼
   4. TEST         ek breakpoint / ek print / ek assert — sirf yeh check karo
        │
        ▼
   5. CONCLUDE     prediction sahi? → aur andar jao / fix karo
        │          prediction galat? → hypothesis galat, naya banao
        └──────────────► back to 2
```

Yeh loop **folder 43 ke optimization loop** se bilkul milta-julta hai
(measure → hypothesize → change one → re-measure). Wahi discipline:

| Optimization (folder 43) | Debugging (yeh folder) |
|---|---|
| Baseline number pehle | Reproduction pehle |
| Ek change at a time | Ek hypothesis test at a time |
| Re-measure same harness | Re-run same input |
| Explain kya badla | Explain bug kyun tha (taaki dobara na ho) |

### Bura debugging kaisa dikhta hai

- Code ko ghoorna aur "hmm" bolna (koi hypothesis nahi)
- Random jagah `printf` daalna bina yeh soche ki **kya expect kar rahe ho**
- Ek saath 4 cheezein badalna, ab kaam karta hai, pata nahi kaunsi thi
- "Compiler ka bug hoga" (99.99% cases mein nahi hai — `## Trap 1`)
- Fix karna bina yeh samjhe ki bug kyun tha ("`+1` hata diya, ab chalta hai")

---

## Step 0 — REPRODUCE karo (warna kuch nahi)

Jo bug tum **reliably trigger nahi kar sakte, use tum fix nahi kar sakte** —
kyunki tumhe pata hi nahi chalega ki fix ne kaam kiya ya bug bas chhup gaya.

Ek achhi reproduction:

- **Deterministic** — har baar chalao, wahi galat output
- **Fast** — 2 second mein, 2 minute mein nahi (tum ise 100 baar chalaoge)
- **Minimal** — sirf woh code jo bug ke liye zaroori hai
- **Automated** — ek command, ek clear pass/fail

Agar bug **intermittent** hai (kabhi hota kabhi nahi) — yeh ek clue hai,
bug nahi gaya. Aksar iska matlab:

| Intermittent bug ka pattern | Sambhavit wajah |
|---|---|
| Multi-threaded, kabhi-kabhi galat | **Data race** (`08`, `26-CONCURRENCY`) |
| Pehli baar theek, phir galat | **Use-after-free** / stale state (`12`, `14-MEMORY/06`) |
| Alag machine pe alag | Uninitialized memory (`12`), ya undefined behaviour |
| `-O2` pe fail, `-O0` pe theek | UB jise optimizer expose karta hai (`05`, `25-OBJECT-MODEL/15`) |
| Input size badhao to fail | Buffer overflow / integer overflow (`12`) |

Intermittent bug ko **loop mein daal ke** deterministic banao:
```bash
for i in $(seq 1 1000); do ./prog || { echo "FAIL run $i"; break; }; done
```

---

## Step 1 — MINIMISE karo (bisection)

50,000-line program mein bug hai. Poora padhna? Nahi. **Aadha kaat do,
dekho bug abhi bhi hai ya nahi.** Repeat. 50,000 → 25,000 → 12,500 → ...
16 steps mein tum 1 line pe pahunch jaate ho (`log2(50000) ≈ 16`).

### Do tarah ki bisection

**(a) Code bisection** — ek badi function mein, aadhe statements comment
karo. Bug gaya? Bug us aadhe mein tha. Nahi gaya? Dusre aadhe mein. Ab us
aadhe ka aadha.

**(b) History bisection — `git bisect`** — "kal tak kaam kar raha tha, aaj
nahi". `git bisect` binary-search karta hai commits mein:

```bash
git bisect start
git bisect bad                 # abhi (HEAD) toota hai
git bisect good v1.4.2         # is tag pe theek tha
# git ab beech ka commit checkout karta hai
./run_test.sh && git bisect good || git bisect bad
# ... 8-10 baar (200 commits ke liye ~log2(200) ≈ 8)
# git batata: "abcd123 is the first bad commit"
git bisect reset
```

Automated: `git bisect run ./run_test.sh` — script exit 0 = good,
non-zero = bad, git khud poora search chala deta hai.

> **HFT relevance:** production trading system mein "yesterday's build
> ne 3 bps zyada slippage diya" — code diff 40 commits ka hai. `git bisect
> run` ek backtest harness ke saath overnight chala do; subah tak woh ek
> commit mil jaata hai jisne fill logic badla. Manual review of 40 commits
> = pura din. Yeh HFT teams ka standard tool hai.

---

## Step 2 — LOCALISE karo (kahan, phir kyun)

Bisection ne bug ko ~10 lines mein la diya. Ab do sawaal, **is order mein**:

1. **KAHAN** state pehli baar galat hota hai? (symptom ki jagah nahi —
   symptom aksar bug se door hota hai. Crash line 900 pe, par galat pointer
   line 40 pe bana tha.)
2. **KYUN** wahan galat hota hai?

Tools jo "kahan" batate hain:
- **Watchpoint** (`03`) — "jab bhi `x` badle, ruk jao" → seedha us line pe
  le jaata hai jo `x` ko galat karti hai
- **Sanitizer** (`06`) — ASan tumhe **exact line** deta hai jahan OOB/UAF
  hua, aur woh line jahan woh memory allocate/free hui thi
- **`assert`** (`23-ERROR-HANDLING/12`) — har function ke start mein apni
  assumptions likho; jo pehle fail hoti hai wahi "kahan" hai
- **Binary search with prints** — ek print beech mein: state yahan theek
  hai? Haan → aage. Nahi → peeche.

---

## Step 3 — Ek cheez badlo, phir dekho

Optimization jaisa hi. Do changes ek saath = pata nahi kaunse ne kaam kiya,
aur ek ne dusre ka side-effect chhupa diya ho sakta hai.

Aur: **fix karne se pehle bug ko samjho.** "`< n` ko `<= n` kiya, ab
chalta hai" — par kyun? Agar tum nahi jaante, to:
- Ho sakta hai tumne symptom dabaaya, bug abhi bhi hai
- Ho sakta hai tumne ek naya bug banaya jo test cover nahi karta
- Tum agli baar **wahi galti** karoge

Fix ke saath ek sentence: *"Loop `<= n` tak jaata tha, `v[n]` out of
bounds hai kyunki valid indices `0..n-1` hain — classic off-by-one, C++
mein arrays 0-indexed aur `size()` last index se ek zyada hai."*

---

## Step 4 — Regression test likho

Bug fix ho gaya. Ab ek test jo **bug wapas aaye to fail ho jaaye**. Warna
6 mahine baad koi refactor karega aur bug silently wapas aa jayega.

```cpp
// Regression: issue #412 — sum_range(v, 0, v.size()) read past end
TEST(SumRange, StopsBeforeEnd) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(sum_range(v, 0, v.size()), 6);   // pehle yeh 6 + garbage tha
}
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "Compiler / OS / library ka bug hoga"

Tumne C++ seekhna shuru kiya ~3 mahine pehle. GCC pe hazaaron engineers ne
30 saal kaam kiya hai. Jab tumhara code galat chalta hai, probability:

```
tumhara bug         : 99.9%
library ka bug      : 0.09%
compiler ka bug     : 0.01%   (aur tab bhi: pehle UB check karo -- 05, 25/15)
```

"Compiler bug" bolne se pehle: `-fsanitize=undefined` clean? Doosre
compiler pe same behaviour? Minimal reproduction jo standard ke against
clearly galat hai? Agar teeno haan — tab, shayad. Warna: tumhara bug.

### Trap 2 — Debugging bina hypothesis

`printf("here 1\n"); printf("here 2\n");` — yeh tab tak bekaar hai jab tak
tum yeh nahi likhte ki **har point pe kya expect karte ho**:
```cpp
printf("after parse: count=%d (expect 8)\n", count);   // <- expectation
```
Ab output ek **test** hai, sirf noise nahi.

### Trap 3 — Symptom ki jagah fix karna

`NullPointerException` line 900 pe. Tum line 900 pe `if (p) p->f();` laga
dete ho. Crash gaya — par ab `p` null **kyun** tha? Woh bug abhi bhi hai,
bas ab silently kuch nahi hota (jo shayad worse hai). Root cause line 40 pe
tha jahan `p` assign hua.

### Trap 4 — "Kaam kar raha hai" = "sahi hai" maan lena

Undefined behaviour "kaam kar sakta hai" — aaj, is compiler pe, is input
pe. `-O2` pe, ya 3 mahine baad, ya customer ki machine pe — nahi. `-Wall
-Wextra -fsanitize=undefined,address` clean hone tak "kaam karta hai" ka
matlab kuch nahi (`06`, `25-OBJECT-MODEL/16`).

### Trap 5 — Reproduction ke bina "fix" karna

Bug intermittent tha. Tumne kuch badla. 5 baar chalaya, nahi aaya. Fixed?
**Nahi pata** — bug 20% baar aata tha, 5 clean runs ki probability
`0.8^5 = 33%`. Pehle reproduction ko reliable (loop, stress, sanitizer)
banao, tab fix verify karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Code ghoorta hoon, mil jaata hai" | Ghoorna = zero hypotheses. Bisect + localise. |
| "Debugger slow hai, print tez hai" | Print bhi tab tak slow hai jab tak expectation na likho. gdb watchpoint aksar 1 step mein answer deta. |
| "Bug fix ho gaya, aage badho" | Regression test likha? Root cause ek line mein likha? |
| "`-O0` pe chalta hai, `-O2` ka bug" | Optimizer ne UB expose kiya. Bug tumhara. `-fsanitize=undefined`. |
| "Intermittent hai, ignore, rare hai" | Rare = race/UB/uninit. Production mein "rare" = 3am page. |

---

## Hands-on

```bash
# reproduction ko deterministic banao (intermittent -> loop)
for i in $(seq 1 500); do ./45-DEBUGGING/examples/04_race_debug || break; done

# code bisection: 01_gdb_practice.cpp mein aadhe kaam comment karke dekho
./build.ps1 45-DEBUGGING/examples/01_gdb_practice.cpp
```

`git bisect` khud try karo: is repo mein ek purana commit checkout karo,
ek jhoota "bad" behaviour maano, `git bisect start / good / bad` chalao,
dekho git kaise commits ke beech jump karta hai. `git bisect reset` se
wapas.

---

## Exercises

1. Ek bug 30% runs mein aata hai. Tum "fix" karte ho aur 4 clean runs
   dekhte ho. Kya probability hai ki bug abhi bhi hai aur tum lucky the?
   <details><summary>Answer</summary>
   4 clean runs agar bug abhi bhi 30% hai: `0.7^4 = 0.24` = **24%**. Yaani
   ek-chauthai chance tumne kuch fix nahi kiya. Chahiye: reproduction ko
   ~100% banao (stress loop / sanitizer / seeded input), tab 1 run bhi
   proof hai.
   </details>

2. "Kal build theek tha, aaj crash." 256 commits beech mein. `git bisect`
   kitne builds test karega (worst case)?
   <details><summary>Answer</summary>
   `ceil(log2(256))` = **8**. Har step search space aadha. 256 → 128 → 64
   → 32 → 16 → 8 → 4 → 2 → 1. Manual: 256 commits padhna. Isliye `git
   bisect run` scriptable regression ke saath itna powerful hai.
   </details>

3. Crash `bt` line 900 pe dikhata hai: `p->value` jahan `p == nullptr`.
   Tumhara pehla instinct hai wahan `if (p)` daalna. Kyun yeh aksar galat
   fix hai, aur behtar pehla kadam kya?
   <details><summary>Answer</summary>
   `if (p)` symptom dabaata hai, `p` null **kyun** hai woh nahi batata —
   ho sakta hai downstream code ko `p` chahiye tha aur ab woh silently
   skip ho raha (naya bug). Behtar: `p` ko **peeche** trace karo — kahan
   set hua? `watch p` laga ke ulta chalao (`09` reverse debug) ya us
   allocation site pe breakpoint. Root cause fix karo (jaha `p` galat set
   hua), symptom line pe nahi.
   </details>

4. Ek program alag laptop pe alag answer deta hai (same input, same
   compiler version). Teen sabse sambhavit wajah?
   <details><summary>Answer</summary>
   (1) **Uninitialized memory** — stack/heap garbage har machine pe alag
   (`12`, `03_memory_bugs`). (2) **UB** jo compiler flags/version pe
   depend karta (`25/15`). (3) **Undefined evaluation order** ya
   platform-dependent type sizes (`int` width, `char` signedness,
   `size_t`). Fix-finding: `-fsanitize=undefined,address` dono machines
   pe.
   </details>

---

## Interview questions

1. Ek intermittent bug diya jaaye — pehle 3 kadam kya?
2. `git bisect` kaise kaam karta hai? Kab useless hota hai? (Ans: jab bug
   har commit pe alag manifest ho, ya "good" baseline pata na ho, ya build
   har commit pe compile na ho.)
3. Crash ka symptom aur uska root cause aksar door kyun hote hain? Ek
   example do.
4. "`-O2` pe crash, `-O0` pe nahi" — kya nikaalte ho is se, aur agla step?
5. Debugging aur scientific method ka mapping batao.

---

## Next
→ [`02-gdb-basics.md`](02-gdb-basics.md)
