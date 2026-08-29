# START-WITH-CLAUDE-CODE.md

Yeh file sirf **aapke liye** hai (learner/builder). Isme likha hai ki Claude Code
ke saath kaise shuru karna hai.

---

## Step 1: Setup

```bash
unzip CPP-MASTERY.zip
cd CPP-MASTERY

# Git shuru karo (recommended -- taaki har batch ke baad checkpoint bane)
git init
git add .
git commit -m "Initial: folders 00-05 complete (Phase 0, 1, 2)"

# VS Code mein kholo
code .
```

VS Code mein Claude Code extension kholo (`Cmd+Esc` / `Ctrl+Esc`).

---

## Step 2: Verify karo ki sab kaam kar raha hai

```bash
make checkall
```

**Expected:** `Compiled OK : 35`, `Failed : 1`

Woh 1 failure **expected** hai — `02-CPP-FIRST-STEPS/examples/06_broken_on_purpose.cpp`
jaan-boojh kar toota hua hai (learner ko fix karna hai).

Agar `make` na chale, aapke system pe `g++` chahiye:
```bash
sudo apt install build-essential    # Ubuntu/WSL
xcode-select --install              # macOS
```

---

## Step 3: Pehla message Claude Code ko

Claude Code `CLAUDE.md` **apne aap padh leta hai** — usme poora context hai.
To aapko lamba prompt likhne ki zarurat nahi.

**Yeh copy-paste karo:**

```
Is repo ka CLAUDE.md padho. Phir Batch 3 banao: folder 06-CONDITIONS.

Process:
1. 06-CONDITIONS/00-README.md padho -- woh spec hai
2. Style match karne ke liye 05-OPERATORS ke 2-3 lessons padho
3. Saare lessons likho (numbered order mein)
4. Examples likho
5. `make folder DIR=06-CONDITIONS` se verify karo
6. Benchmarks chalao aur REAL numbers lessons mein daalo
7. Audit files update karo

Ek baar mein sirf yeh EK folder. Khatam hone pe batao, main review karunga,
phir 07 pe jaayenge.
```

---

## Step 4: Har folder ke baad

```bash
make folder DIR=06-CONDITIONS      # verify
git add . && git commit -m "Folder 06 complete"
```

Phir agla folder:
```
Ab folder 07-LOOPS banao, wahi process follow karke.
```

---

## Practical tips

### Ek baar mein ek folder — poora batch nahi
Claude Code ka context window limited hai. Ek folder = ~10-15 lessons + examples,
yeh comfortable size hai. Poora batch maangoge to quality gir jaayegi ya context
khatam ho jaayega.

### Benchmarks pe zor do
Agar Claude Code koi performance claim likhe bina chalaye, use bolo:
```
Yeh benchmark actually chalao aur REAL number daalo. Aur verify karo ki
benchmark sirf wahi cheez measure kar raha hai jo claim kar rahe ho.
```

### Compile verification maangoge to hi milegi
Har folder ke baad explicitly bolo:
```
make folder DIR=... chalao aur output dikhao
```

### Agar style drift kare
```
05-OPERATORS/05-bitwise-operators.md padho aur usi depth/format mein likho.
```

### Context bhar jaye to
Naya session shuru karo. `CLAUDE.md` phir se auto-load ho jaayega, aur
`git log` se pata chal jaayega kahan tak pahunche the.

---

## Kaunse folders sabse important hain

Agar aap sab nahi bana sakte, to yeh priority order hai:

1. **`12-POINTERS`, `14-MEMORY`** — yahan se "asli C++" shuru hoti hai
2. **`17-RAII`, `18-COPY-MOVE`** — modern C++ ka dil
3. **`19-STL`** — sabse bada folder, sabse zyada use hoga
4. **`26-CONCURRENCY`, `27-ATOMICS`** — HFT ka prerequisite
5. **`32-CACHE`, `35-PROFILING`** — performance engineering ka core
6. **`36-LOW-LATENCY`, `39-ORDER-BOOK`, `44-HFT-PROJECTS`** — HFT track ka core

---

## Progress tracking

`00-START-HERE/BUILD-STATUS.md` mein checklist hai. Har folder complete karne pe
`[ ]` ko `[x]` karo.

Aur `00-START-HERE/WHAT-I-STILL-NEED-TO-LEARN.md` gap tracker hai — Claude Code
ko har batch ke baad usse update karne ko bolo.
