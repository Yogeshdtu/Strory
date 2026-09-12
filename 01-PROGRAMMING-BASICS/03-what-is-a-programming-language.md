# 03 — Programming language kya hai?

## Prerequisites
`01-what-is-a-computer.md`, `02-what-is-programming.md`

## Yeh topic abhi kyun
Aap jaante ho ki programming = instructions dena. Ab sawal: **kis bhasha mein?**
CPU Hindi nahi samajhti, English nahi samajhti. Toh phir?

---

## Problem: CPU sirf numbers samajhti hai

CPU ke liye har instruction ek **number** hai. Literally.

Agar aap CPU ko bolna chahte ho "5 aur 3 jodo", to actual instruction kuch aisi dikhegi:

```
10110000 00000101
00000100 00000011
```

Yeh **machine code** hai. Yahi asli bhasha hai jo CPU samajhti hai.

Aur haan — log **sach mein** aise programming karte the, 1940s-50s mein. Switches
flip karke, punch cards se. Ek chhoti si galti aur hafton ka kaam barbaad.

---

## Solution: abstraction layers

Insaanon ne socha: "hum aisi bhasha banate hain jo humein samajh aaye, aur ek program
banate hain jo usko machine code mein badal de."

```
  LEVEL 3:  HIGH-LEVEL LANGUAGE           <- insaan ke liye aasan
            C++, Python, Java, Rust
            int sum = a + b;
                    |
                    | (compiler)
                    v
  LEVEL 2:  ASSEMBLY LANGUAGE             <- machine code ka readable version
            mov eax, [a]
            add eax, [b]
                    |
                    | (assembler)
                    v
  LEVEL 1:  MACHINE CODE                  <- CPU ki asli bhasha
            10001011 01000101 11111000
                    |
                    v
  LEVEL 0:  ELECTRICITY (transistors)     <- physical reality
            on / off, high / low voltage
```

Har layer neeche wali layer ko chhupati hai. Isko **abstraction** kehte hain — programming
ka sabse bada idea.

---

## Level 1: Machine code

- Pure binary (0s aur 1s)
- Har CPU family ka apna alag hota hai (x86-64 aur ARM ka machine code alag hai)
- Insaan ke liye padhna practically impossible
- Sabse tez (obviously — yahi to CPU chalati hai)

---

## Level 2: Assembly

Machine code ko human-readable naam de diye. Ek assembly instruction ≈ ek machine
instruction.

```asm
mov eax, 5      ; eax register mein 5 daalo
add eax, 3      ; usme 3 jodo
```

- Ab bhi CPU-specific hai
- Ab bhi bahut low-level (ek line = ek chhota kaam)
- Lekin padha ja sakta hai

> **HFT relevance:** HFT engineers **assembly padhte hain** — likhte kam hain.
> Jab aap kisi function ko optimize kar rahe ho, aap compiler ka output assembly mein
> dekhte ho: "compiler ne kya banaya? kya usne mera loop vectorize kiya? kya usne
> function inline kiya?" Folder 34 mein poora seekhoge.

---

## Level 3: High-level languages

Ab hum insaan ki tarah likh sakte hain:

```cpp
int sum = a + b;
```

Ek line — compiler isko kai machine instructions mein badal dega.

**Fayde:**
- Padhna/likhna aasan
- Portable (same code alag CPUs pe chal sakta hai, recompile karke)
- Kam galtiyan
- Tez development

**Nuksaan:**
- Aapka control kam ho jaata hai
- Kuch languages mein performance kharab ho jaati hai

---

## Languages ka spectrum

```
  LOW LEVEL                                               HIGH LEVEL
  (hardware ke paas)                                (insaan ke paas)

  Assembly ---- C ---- C++ ---- Rust ---- Java ---- Python ---- Excel
     |          |       |         |         |          |
   full      manual   manual    safe     garbage    automatic
  control    memory   memory   memory   collected     sab kuch
                    + abstract
```

**C++ ki special jagah:** C++ high-level abstractions deta hai (classes, templates,
STL) **lekin** low-level control bhi deta hai (manual memory, pointers, inline assembly).

Isko **"zero-cost abstraction"** kehte hain — aap achha code likh sakte ho bina
performance khoye. Yahi wajah hai ki HFT, game engines, operating systems, browsers —
sab C++ mein bane hain.

---

## Language ke 3 hisse

Har programming language mein yeh 3 cheezein hoti hain:

### 1. Syntax — grammar rules
Kya likha ja sakta hai, kaise likha ja sakta hai.
```cpp
int x = 5;     // sahi syntax
int x = 5      // galat syntax (semicolon nahi hai)
5 = x int;     // bilkul bakwaas
```

### 2. Semantics — matlab
Likhe hue code ka matlab kya hai.
```cpp
x = x + 1;     // syntax sahi, semantics: x ki value ek badha do
```

### 3. Standard library — ready-made tools
Language ke saath aane wale pre-built functions/classes.
C++ mein: `std::vector`, `std::string`, `std::sort`, etc.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++ Python se hamesha tez hai" | Aksar haan, lekin galat C++ code slow bhi ho sakta hai |
| "Assembly hamesha C++ se tez hai" | Nahi! Modern compilers insaanon se behtar assembly likhte hain, 95% cases mein |
| "High-level = kharab" | High-level = alag trade-off. Sahi jagah sahi tool |
| "Machine code aur assembly same hai" | Assembly = machine code ka text version. 1:1 mapping, par same nahi |

---

## Exercises

1. Yeh 3 languages ko low→high order mein lagao: Python, Assembly, C++.

2. Machine code CPU-specific kyun hota hai? (Hint: Intel aur ARM ke instruction sets alag hain)

3. Godbolt.org kholo, yeh code paste karo (C++ chuno, right side mein assembly dikhega):
   ```cpp
   int add(int a, int b) {
       return a + b;
   }
   ```
   Kitni assembly lines bani? Ab `-O2` flag daalo (compiler options box mein) — kya badla?
   <details><summary>Kya hoga</summary>
   `-O0` (default) pe 6-8 lines aayengi jisme stack pe values save hongi.
   `-O2` pe sirf 2-3 lines: `lea eax, [rdi+rsi]` aur `ret`. Compiler ne sab optimize kar diya.
   Yeh aapka pehla "compiler kya karta hai" moment hai. 🎉
   </details>

4. "Abstraction" ko apne shabdon mein define karo. Ek real-life example do (programming se hatke).
   <details><summary>Example</summary>
   Car chalana. Aap steering, accelerator, brake use karte ho. Aapko engine ke andar
   pistons, valves, fuel injection ka kuch nahi pata. Car ka interface = abstraction.
   </details>

---

## Next
→ [`04-source-code-and-files.md`](04-source-code-and-files.md)
