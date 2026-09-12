# Is course ko use kaise karein

## Prerequisites
Kuch nahi. Bas `README.md` padh liya ho.

## Yeh file kyun

Kyunki **seekhne ka tarika**, seekhne ki **cheez** se zyada important hota hai.
Do log same book padhte hain — ek expert ban jaata hai, doosra kuch nahi seekh paata.
Fark method ka hota hai.

---

## 1. Active learning vs Passive learning

**Passive learning** (kaam nahi karta):
- Video dekhna
- Tutorial padhna
- Code copy-paste karke chalana
- "Samajh aa gaya" feel karna

**Active learning** (yahi kaam karta hai):
- Code apne haath se type karna
- Output **pehle guess karna**, phir chalana
- Program ko jaan-boojh kar todna
- Bina dekhe wapas likhne ki koshish karna
- Kisi aur ko explain karna (ya khud ko, zor se bolke)

Yeh course active learning ke liye design kiya gaya hai. Har file mein
**"Output kya aayega? Guess karo"** type ke boxes milenge. Unhe skip mat karna.

---

## 2. Daily routine (suggested)

Agar aap roz 2 ghante de sakte ho:

```
0:00 - 0:15   Pichle din ka revision (notes padho, code dobara likho)
0:15 - 1:00   Naya topic padho (1-2 lesson files)
1:00 - 1:40   Examples type karo, chalao, todo
1:40 - 2:00   Exercises solve karo
```

Agar sirf 1 ghanta hai:

```
0:00 - 0:10   Revision
0:10 - 0:40   Naya topic + examples
0:40 - 1:00   Exercises
```

**Consistency > intensity.** Roz 1 ghanta > hafte mein ek din 7 ghante.

---

## 3. "Samajh nahi aaya" — tab kya karein?

Yeh normal hai. Har programmer ke saath hota hai. Steps:

### Step 1 — Prerequisites check karo
File ke top pe prerequisites likhe hain. Kya woh sab clear hai? Agar nahi, wahan wapas jao.

### Step 2 — Chhota kar do
Poora example samajh nahi aa raha? Usme se 3 lines nikaal ke alag file mein daalo,
chalao, dekho kya hota hai.

### Step 3 — Print karke dekho
```cpp
std::cout << "yahan tak pahuncha, x = " << x << "\n";
```
Yeh sabse purana aur sabse effective debugging tool hai. Har jagah `cout` daal do
aur dekho program kya kar raha hai.

### Step 4 — Diagram banao
Kagaz uthao. Memory boxes banao. Arrows banao. Pointers/references/objects — sab
kuch diagram se clear ho jaata hai.

### Step 5 — Aage badh jao, wapas aao
Kabhi kabhi ek concept tab samajh aata hai jab aap uske aage ka concept dekh lete ho.
Agar 30 minute se atke ho, ek mark laga do aur aage badho. 2 din baad wapas aao.

### Step 6 — Rubber duck
Ek khilona/duck/deewaar ke saamne baith ke **zor se** explain karo ki code kya kar
raha hai, line by line. Aadha time aapko khud hi galti mil jayegi. Yeh real technique
hai, mazaak nahi — ise "rubber duck debugging" kehte hain.

---

## 4. Compiler errors ko kaise padhein

Beginner log compiler error dekh ke ghabra jaate hain. Ghabrane ki zarurat nahi.

```
main.cpp:5:5: error: 'cout' was not declared in this scope
    5 |     cout << "Hi";
      |     ^~~~
```

Ise todo:

| Part | Matlab |
|------|--------|
| `main.cpp` | kis file mein |
| `:5` | line number 5 |
| `:5` (doosra) | column number 5 |
| `error:` | yeh error hai (warning nahi) |
| `'cout' was not declared in this scope` | actual problem |
| `^~~~` | exactly kahan point kar raha hai |

**Golden rule: SABSE PEHLA error thik karo, phir dobara compile karo.**

Ek galti se 20 errors aa sakte hain. Pehla thik karne pe baaki 19 apne aap gayab ho
jaate hain. Neeche wale errors ko ignore karo shuru mein.

---

## 5. Warnings ko hamesha ON rakho

Hamesha aise compile karo:

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -g program.cpp -o program
```

| Flag | Kya karta hai |
|------|---------------|
| `-std=c++20` | C++20 standard use karo |
| `-Wall` | common warnings dikhao |
| `-Wextra` | aur bhi warnings dikhao |
| `-Wpedantic` | strictly standard follow karo |
| `-g` | debug info daalo (debugger ke liye) |

Warnings ko **error jaisa treat karo**. 90% bugs warnings mein pehle hi dikh jaate hain.

HFT firms mein codebases aksar `-Werror` ke saath build hoti hain — matlab koi bhi
warning = build fail. Aadat abhi se daal lo.

---

## 6. Notes kaise banayein

Har folder mein apni `MY-NOTES.md` file banao:

```markdown
# Folder 12 - Pointers - Mere notes

## Confusions
- `int* p` aur `int *p` mein fark? -> koi nahi, sirf style
- `*p` do jagah use hota hai: declaration mein aur dereference mein  <- yeh confuse karta tha

## Yaad rakhne wali baatein
- `&` = "address of"
- `*` = "value at"
- nullptr = "kahin point nahi kar raha"

## Galtiyan jo maine ki
- freed pointer ko dobara use kiya -> crash
- uninitialized pointer -> garbage address
```

6 mahine baad yeh notes gold ban jaate hain.

---

## 7. Practice kahan karein (course ke bahar)

Jab aap folder 08 (functions) tak pahunch jao, tab se:

| Site | Kis liye |
|------|----------|
| [Compiler Explorer (godbolt.org)](https://godbolt.org) | assembly dekhne ke liye — folder 33/34 se mandatory |
| [cppreference.com](https://en.cppreference.com) | official reference — Google se pehle yahan dekho |
| LeetCode (Easy → Medium) | DSA practice — folder 20 se |
| Codeforces | speed + problem solving |
| [quick-bench.com](https://quick-bench.com) | micro-benchmarks — folder 35 se |

**Note:** Abhi shuru mein LeetCode mat karo. Pehle basics. LeetCode folder 20 se relevant hai.

---

## 8. Kya NAHI karna hai

| ❌ Galti | Kyun galat hai |
|---------|----------------|
| "Main pehle poori theory padh lunga, phir code karunga" | Theory bina code ke bhoolti hai |
| Tutorial hell — 10 alag courses ek saath | Ek path pakdo, complete karo |
| ChatGPT/Claude se code likhwana, samjhe bina | Aap seekhoge nahi, sirf deliver karoge |
| C++ ke saath Java/Python bhi ek saath seekhna | Focus toot jaata hai. Pehle ek |
| Advanced topics pe jump karna | C++ mein yeh sabse badi galti hai |
| Errors se darna | Errors normal hain, roz aate hain |

AI tools use karna galat nahi hai — lekin **pehle khud try karo, phir AI se poocho
"maine yeh kyun galat kiya?"**. Answer maang lena seekhna nahi hai.

---

## 9. Progress kaise measure karein

Har phase ke end mein khud se poocho:

- [ ] Kya main is folder ke sabhi examples bina dekhe likh sakta hoon?
- [ ] Kya main kisi aur ko yeh concept explain kar sakta hoon?
- [ ] Kya maine sabhi exercises solve kiye?
- [ ] Kya challenge complete kiya?
- [ ] Kya main interview questions ka jawab de sakta hoon?

Agar 4/5 haan hai — aage badho.
Agar 2/5 hai — folder dobara karo. Sharam ki baat nahi hai.

---

## Next

→ [`02-full-curriculum-map.md`](02-full-curriculum-map.md) — poora syllabus dekhne ke liye
→ [`03-setup-your-machine.md`](03-setup-your-machine.md) — compiler install karne ke liye
