# 00 — START HERE

Namaste. Aap bilkul sahi jagah pe ho.

Yeh file aapka **entry point** hai. Ise pura padho — 10 minute lagenge — phir aapko pata chal
jayega ki aage kya karna hai.

---

## 1. Yeh course kiske liye hai?

Yeh course maan kar chalta hai ki:

- Aapne **kabhi programming nahi ki** (ya thodi bahut ki hai, koi baat nahi)
- Aapko **compiler kya hota hai** yeh bhi nahi pata
- Aapko **terminal** se dar lagta hai
- Aur aap fir bhi **HFT / low-latency C++ engineer** banna chahte ho

Agar aapko C++ already aata hai, tab bhi yeh course useful hai — bas aap shuru ke folders
skip kar sakte ho. Lekin **mera suggestion hai ki skip mat karo**. Bahut se "experienced"
C++ programmers ko bhi `int x = 5;` mein exactly kya ho raha hai, yeh nahi pata hota.
Aur HFT interviews mein wahi cheezein poochi jaati hain.

---

## 2. Yeh course kaisa dikhta hai (roadmap)

```
                 AAP YAHAN HO
                      |
                      v
  [ PHASE 0 ]  Computer + Programming Basics          folder 01
       |       "computer kya hai, program kya hai"
       v
  [ PHASE 1 ]  Absolute C++ Beginner                  folder 02
       |       "Hello World ka har ek character"
       v
  [ PHASE 2 ]  Core C++ Fundamentals                  folders 03-05
       |       variables, types, I/O, operators
       v
  [ PHASE 3 ]  Control Flow + Functions               folders 06-08
       |       if/else, loops, functions
       v
  [ PHASE 4 ]  Arrays + Strings + Structs             folders 09-11
       |
       v
  [ PHASE 5 ]  Pointers + References + Memory         folders 12-14
       |       <-- yahan se "real C++" shuru hota hai
       v
  [ PHASE 6 ]  Classes + OOP                          folders 15-16
       |
       v
  [ PHASE 7-8] RAII + Copy/Move Semantics             folders 17-18
       |       <-- yeh C++ ka dil hai
       v
  [ PHASE 9-10] STL + Algorithms + DSA                folders 19-20
       |
       v
  [ PHASE 11-13] Templates + Modern C++ + Errors      folders 21-23
       |
       v
  [ PHASE 14-15] Build Systems + Object Model         folders 24-25
       |
       v
  [ PHASE 16-17] Concurrency + Atomics + Lock-Free    folders 26-28
       |
       v
  [ PHASE 18-19] Linux Systems + Networking           folders 29-30
       |
       v
  [ PHASE 20-23] CPU + Cache + Compiler + Profiling   folders 31-35
       |       <-- "performance engineer" banne ka phase
       v
  [ PHASE 24 ]  Ultra-Low-Latency C++                 folder 36
       |
       v
  [ PHASE 25-32] HFT TRACK                            folders 37-44
       |       market data -> order book -> matching engine
       |       -> HFT concurrency -> HFT networking -> projects
       v
  [ PHASE 33-34] Interview Prep + Final Audit         folders 45-48
       |
       v
                 HFT C++ ENGINEER
```

---

## 3. Kitna time lagega? (honest answer)

Main aapse jhooth nahi bolunga.

| Stage | Folders | Realistic time (daily 2 ghante) |
|-------|---------|-------------------------------|
| Absolute beginner → comfortable | 01–11 | 2–3 mahine |
| Pointers → OOP → RAII → move | 12–18 | 2–3 mahine |
| STL + DSA + templates + modern | 19–23 | 3–4 mahine |
| Systems + concurrency | 24–30 | 3–4 mahine |
| Performance engineering | 31–36 | 3–4 mahine |
| HFT track | 37–48 | 4–6 mahine |

Total: **realistically 15–24 mahine** serious, daily practice ke saath.

Koi aapse bolta hai "3 mahine mein HFT C++ seekh lo" — woh jhooth bol raha hai.
HFT firms (Jane Street, Optiver, Jump, Citadel, Tower, HRT, IMC, Quadeye, Graviton, etc.)
mein log 5–10 saal ka experience leke jaate hain.

**Lekin** — yeh course aapko woh 5–10 saal ka structured path de raha hai. Zyada tar log
isliye fail hote hain kyunki unhe pata hi nahi hota ki aage kya padhna hai. Aapke saath
woh problem nahi hogi.

---

## 4. Is course ko use kaise karein (RULES)

Yeh sabse important section hai. Please dhyaan se padho.

### Rule 1 — Code ko *type* karo, copy-paste mat karo

Har `.cpp` example ko apne haath se type karo. Typing se muscle memory banti hai.
Copy-paste se kuch nahi banta.

### Rule 2 — Har program ko *chalao*

Padhne se samajh nahi aata. Chalane se aata hai. Har example ko compile karo, run karo,
output dekho.

### Rule 3 — Program ko *todo* (break it)

Yeh sabse powerful technique hai. Program chal gaya? Ab usko jaan-boojh kar todo:
- semicolon hata do — kya error aata hai?
- `int` ko `double` kar do — output kya badalta hai?
- `return 0;` hata do — kya hota hai?

Errors se darna nahi hai. Errors hi teacher hain.

### Rule 4 — Har file ke end mein exercises hain — unhe skip mat karo

Exercises ke bina yeh course sirf padhai hai, seekhna nahi.

### Rule 5 — Prerequisites ko respect karo

Har file ke top pe likha hoga "**Prerequisites**". Agar woh cheezein aapko nahi aati,
to pehle wahan jao. Aage bhaagne se kuch nahi hoga — C++ mein har cheez pichli cheez pe
khadi hai.

### Rule 6 — Notes banao

Har folder mein aap apna `MY-NOTES.md` bana sakte ho. Jo samajh na aaye, wahan likho.
Baad mein wapas aao.

---

## 5. File format — har lesson mein kya milega

Har theory file ka structure yeh hai:

```markdown
# Topic ka naam

## Prerequisites        <- kya pehle se aana chahiye
## Yeh topic abhi kyun  <- iss point pe kyun padha rahe hain
## What / Kya hai       <- definition, simple Hinglish
## Why / Kyun chahiye   <- motivation
## Intuition            <- real-world analogy
## Syntax               <- exact syntax
## Examples             <- chhote se bade
## Andar kya hota hai   <- internal working / memory
## Common mistakes      <- galtiyan jo sab karte hain
## Edge cases           <- tricky cheezein
## Performance          <- speed pe kya asar
## HFT relevance        <- HFT mein iska kya role hai
## Exercises            <- practice
## Interview questions  <- jo interview mein poocha jaata hai
## Challenge            <- ek bada task
## Next                 <- aage kya padhna hai
```

Beginner files mein "Performance" aur "HFT relevance" sections chhote honge —
kyunki abhi context nahi hai. Aage jaake woh sections bade hote jayenge.

---

## 6. Is folder ki files

| File | Kya hai |
|------|---------|
| `README.md` | Yeh file — entry point |
| `01-how-to-use-this-course.md` | Study method, detail mein |
| `02-full-curriculum-map.md` | Poora syllabus, folder by folder |
| `03-setup-your-machine.md` | Compiler + editor + terminal setup (Windows/Mac/Linux) |
| `04-glossary.md` | Har technical term ka Hinglish meaning |
| `CPP-COMPLETENESS-AUDIT.md` | C++ language + STL coverage checklist |
| `HFT-COMPLETENESS-AUDIT.md` | HFT topics coverage checklist |
| `WHAT-I-STILL-NEED-TO-LEARN.md` | Gap tracker — kya abhi bhi baaki hai |
| `BUILD-STATUS.md` | Repo ka construction status |

---

## 7. Ab kya karein? (aapka pehla step)

1. Pehle `03-setup-your-machine.md` padho aur compiler install karo. **Yeh mandatory hai.**
   Bina compiler ke aap kuch nahi kar paoge.
2. Phir `01-how-to-use-this-course.md` padho.
3. Phir seedha jao: **[`../01-PROGRAMMING-BASICS/00-README.md`](../01-PROGRAMMING-BASICS/00-README.md)**

Chaliye shuru karte hain. 🚀

---

## Next

→ [`03-setup-your-machine.md`](03-setup-your-machine.md)
