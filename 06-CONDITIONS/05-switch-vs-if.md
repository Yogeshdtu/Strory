# 05 — `switch` vs `if` — compiler kya banata hai

## Prerequisites
- [`04-switch-statement.md`](04-switch-statement.md)
- `05-OPERATORS/09-precedence-associativity.md` mein `-S` assembly dekha tha
- `00-START-HERE/03-setup-your-machine.md` (compile flags)

## Yeh topic abhi kyun
"`switch` `if` se tez hota hai" — yeh aadha sach hai. Sach yeh hai: **depends on
kaunse case values hain, aur compiler kitna smart hai.** Is lesson mein hum actual
assembly dekhenge (`g++ -O2 -S`) aur ek benchmark chalayenge — taaki guess karne ke
bajaay **pata** ho.

Yeh assembly padhne (folder 34) aur compiler optimization (folder 33) ka pehla
proper taste bhi hai.

---

## Compiler ke paas 4 options

`switch` (ya usse mil-ta julta `if/else if` chain) ke liye compiler `-O2` pe in
mein se kuch banata hai:

| Strategy | Kab | Cost |
|---|---|---|
| **Arithmetic** | cases + results mein pattern ho | ~2–3 instructions, no branch |
| **Jump table** | case values **dense** hon (0,1,2,3,…) | O(1) — ek indexed indirect jump |
| **Binary search of compares** | case values **bikhre** hon (1, 97, 5000) | O(log n) compares |
| **Sequential compares** | bahut kam cases | O(n) — `if/else` jaisa |

Chunav case values ki **density** pe depend karta hai, `switch` vs `if` keyword pe
nahi.

---

## Dekhte hain — DENSE cases → JUMP TABLE

```cpp
extern void a(); extern void b(); /* ... h() */

void dispatch_switch(int x) {
    switch (x) {
        case 0: a(); break;   case 1: c(); break;
        case 2: h(); break;   case 3: b(); break;
        case 4: e(); break;   case 5: d(); break;
        case 6: g(); break;   case 7: f(); break;
    }
}
```

`g++ -std=c++20 -O2 -S -masm=intel` (GCC 15, x86-64) →

```asm
dispatch_switch(int):
        cmp     ecx, 7                          ; x > 7 ?  (unsigned compare, bounds check)
        ja      .L1                             ; haan -> kuch mat karo
        lea     rdx, .L4[rip]                   ; jump table ka address
        movsx   rax, DWORD PTR [rdx+rcx*4]      ; table[x] -> offset (x se index)
        add     rax, rdx
        jmp     rax                             ; INDIRECT JUMP -> seedha sahi case
.L4:                                            ; table: 8 offsets
        .long   .L11-.L4    ; case 0 -> a()
        .long   .L10-.L4    ; case 1 -> c()
        ...
.L1:
        ret
```

**Kaam:** ek bounds check (`cmp`/`ja`), ek memory load (table se), ek indirect
`jmp`. **`x` 0 ho ya 7 — same cost.** O(1). 100 cases hote to bhi same.

### Aur ab wahi cheez `if/else if` chain se

```cpp
void dispatch_chain(int x) {
    if      (x == 0) a();
    else if (x == 1) c();
    else if (x == 2) h();
    /* ... */
    else if (x == 7) f();
}
```

`-O2` pe GCC 15 ka output:

```asm
dispatch_chain(int):
        cmp     ecx, 7
        ja      .L13
        lea     rdx, .L16[rip]
        movsx   rax, DWORD PTR [rdx+rcx*4]
        add     rax, rdx
        jmp     rax
        ...
```

**Bilkul same jump table.** 😲 GCC ne dense integer `if`-chain ko pehchana aur wahi
optimization laga di.

> **Sabak 1:** dense integer dispatch ke liye, modern compiler pe `switch` aur
> `if/else if` chain **same machine code** de sakte hain. `switch` isliye choose
> karo — **readability** aur **`-Wswitch` completeness check** — performance ke liye
> nahi.

---

## Aur bhi smart — ARITHMETIC

```cpp
int dense(int x) {
    switch (x) {
        case 0: return 100;   case 1: return 200;
        case 2: return 300;   /* ... */   case 7: return 800;
        default: return -1;
    }
}
```

`-O2` →

```asm
dense(int):
        cmp     ecx, 7
        ja      .L3
        add     ecx, 1
        imul    eax, ecx, 100        ; result = (x + 1) * 100
        ret
.L3:
        mov     eax, -1
        ret
```

Koi jump table bhi nahi — compiler ne dekha `result == (x+1)*100` aur **formula**
likh diya. `if`-chain version se bhi yehi banta hai. **Yeh guess karke likhna
impossible hai — isiliye measure/inspect karte hain.**

---

## SPARSE cases → jump table nahi ban sakta

```cpp
int sparse(int x) {
    switch (x) {
        case 1:     return 100;
        case 97:    return 200;
        case 5000:  return 300;
        case 99999: return 400;
        default:    return -1;
    }
}
```

Values `1, 97, 5000, 99999` — beech mein 99998 khaali slots. Jump table = ~400 KB
array, bekaar. Compiler **binary search of compares** banata hai:

```asm
sparse(int):
        cmp     ecx, 5000
        je      .L8              ; mid pe check
        jg      .L7             ; > 5000 -> upar wala half
        ; <= 5000 wala half: 1 aur 97 check
        cmp     ecx, 1
        je      .L5
        cmp     ecx, 97
        ...  (cmove -- branchless)
.L7:    cmp     ecx, 99999
        ...
```

O(log n) — 4 cases ke liye ~2 compares. Chain se behtar (jo O(n) = 4 compares hota),
par jump table jitna tez nahi.

---

## Benchmark — jump table vs O(n) scan

`switch` on random key `0..31` (compiler → jump table) vs same mapping ek 32-entry
linear scan se (`for (i) if (keys[i] == x) return vals[i];`):

```
switch (jump table): ~300 ms
linear scan (O(n))  : ~2450 ms
                      --------
                      ~8x
```

*(GCC 15.1, `-O2`, x86-64, 16384 random keys × 4000 reps. Aapke machine pe number
alag, ratio similar.)*

**Kyun 8x:** linear scan average `16` comparisons + branch mispredicts karta hai
(random key). Jump table = 1 bounds check + 1 load + 1 indirect jump, hamesha.

⚠️ `-O0` pe yeh benchmark bekaar hai — wahan `switch` bhi naive compares banta hai.
Benchmark hamesha `-O2`.

---

## Toh decision kaise loge

```
Integer / enum value ko constants se match karna hai?
│
├─ cases DENSE (0,1,2,… ya paas-paas) ────────► switch
│     compiler jump table / arithmetic banata hai (O(1))
│     + enum class pe -Wswitch completeness check
│
├─ cases SPARSE par fixed ───────────────────► switch
│     compiler binary search banata hai (O(log n))
│     phir bhi hand-written chain se behtar + readable
│
├─ ranges chahiye (x > 5 && x < 10) ─────────► if / else if
│     switch ranges support nahi karta
│
├─ float / string / complex bool ────────────► if / else if
│     ya string ke liye std::unordered_map / hashing
│
└─ 2-3 cases, hot path, profiler ne bola ────► measure dono, -O2 pe -S dekho
```

**Default:** integer/enum dispatch → `switch`. Baaki → `if`.

> **HFT relevance:** Market-data feed handlers aur matching-engine event loops
> message-type byte / `enum` pe `switch` karte hain — jump table O(1) dispatch, aur
> naye message types add karne pe `-Wswitch` compile-time reminder. Jab cases sparse
> ya string-keyed hon (e.g. FIX tag dispatch), perfect-hash / `constexpr` lookup
> tables use hote hain — runtime `if`-chain se bacha jaata hai. Folders 33, 38–40.

---

## Khud inspect karo

```bash
# repo root se (Windows):
./build.ps1 asm 06-CONDITIONS/examples/02_switch_demo.cpp

# Linux / Git Bash:
make asm FILE=06-CONDITIONS/examples/02_switch_demo.cpp

# manual:
g++ -std=c++20 -O2 -S -masm=intel yourfile.cpp -o - | c++filt | less
```

Ya online: **Compiler Explorer** (godbolt.org) — left mein code, right mein live
assembly, colour-coded. HFT engineers isko roz use karte hain.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "`switch` hamesha tez"
Dense pe: aksar same as chain (dono jump table). Sparse pe: better than chain, par
O(log n). 2-3 cases pe: koi farq nahi.

### Trap 2 — `-O0` pe benchmark karna
`-O0` pe koi optimization nahi — `switch` bhi sequential compares. Poora comparison
jhootha. **Hamesha `-O2`.**

### Trap 3 — Jump table ko "muft O(1)" samajhna
Indirect `jmp` branch predictor ke liye mushkil hota hai jab target har baar
badalta ho (random dispatch). Predictable pattern pe hi truly free.

### Trap 4 — Micro-optimizing bina profile ke
Jab tak profiler (folder 35) ne yeh function hot na bataya ho, `switch` vs `if`
sirf readability ka faisla hai. Pehle sahi likho, phir maapo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`switch` `if` se hamesha tez" | Dense pe aksar identical machine code |
| "`if`-chain kabhi jump table nahi banti" | Modern GCC/Clang dense chain ko table banati hai |
| "Jump table har `switch` ke liye" | Sirf dense cases; sparse → binary search |
| "`-O0` benchmark valid hai" | Nahi — optimizations off, comparison meaningless |
| "Assembly padhna advanced hai, abhi zaroorat nahi" | `-S` / godbolt = guess hatane ka sabse seedha tareeka |

---

## Exercises

1. **Inspect:** yeh do functions ek file mein likho, `g++ -O2 -S -masm=intel` se
   assembly nikaalo. Same hai ya alag?
   ```cpp
   int f(int x) { switch (x) { case 0:return 10; case 1:return 20;
                               case 2:return 30; case 3:return 40; default:return 0; } }
   int g(int x) { if (x==0) return 10; if (x==1) return 20;
                  if (x==2) return 30; if (x==3) return 40; return 0; }
   ```

2. **Dense vs sparse:** `f()` mein case values `0,1,2,3` rakho, phir `1,1000,50000,
   9999999` rakho. Dono ki assembly compare karo. Kya badla (`jmp rax` vs `cmp`
   chain)?

3. **Arithmetic pattern:** `switch (x) { case i: return i*i; }` (0..7 tak) likho.
   `-O2` assembly mein jump table dikha ya `imul`?

4. **Benchmark:** ek `enum` (8 values) pe `switch` dispatch aur 8-long `if/else if`
   chain — dono ko random enum values pe `-O2` pe time karo. Farq measurable hai?
   Ratio likho.

5. **Kab `switch` nahi:** in mein se `switch` kis pe use nahi kar sakte —
   `int day`, `double weight`, `char cmd`, `std::string name`, `enum class State`.
   Har "nahi" ka reason.
   <details><summary>Answer</summary>
   `double weight` (non-integral), `std::string name` (non-integral). Baaki teenon
   `switch`-able.
   </details>

6. **godbolt:** [godbolt.org](https://godbolt.org) pe exercise 1 ka code daalo,
   compiler `x86-64 gcc 15`, flags `-O2`. Assembly ko code ke saath colour-match
   karke padho.

---

## Interview questions

1. `switch` ko compiler kin-kin tareekon se implement karta hai? Kaunsa kab?
2. Jump table kya hai? Uski complexity? Kab banti hai?
3. Kya `if/else if` chain kabhi jump table ban sakti hai? (Haan — kwhen?)
4. Sparse case values pe `switch` ka kya hota hai?
5. `switch` vs `if` — performance ke liye choose karna sahi hai? Kab readability se decide karo?
6. `-O0` pe `switch` benchmark karna kyun galat hai?
7. Indirect jump (jump table) branch predictor ke liye kyun challenging hai?

---

## Next
→ [`06-if-with-initializer.md`](06-if-with-initializer.md)
