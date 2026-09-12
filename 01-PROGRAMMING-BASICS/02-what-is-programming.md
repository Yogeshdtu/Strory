# 02 — Programming kya hai?

## Prerequisites
`01-what-is-a-computer.md`

## Yeh topic abhi kyun
Computer ke hisse pata chal gaye. Ab samajhte hain ki hum usse **kaam karwate kaise hain**.

---

## Simple jawab

**Programming = computer ko step-by-step instructions dena.**

Bas itna hi. Aur kuch nahi.

---

## Intuition: recipe wala example

Maan lo aapko kisi ko chai banana sikhana hai jo **bilkul kuch nahi jaanta** aur
**bilkul literal** hai — jo bologe wahi karega, ek inch idhar-udhar nahi.

Aap yeh nahi keh sakte:
> "Chai bana do"

Kyunki usko nahi pata "chai" kya hai. Aapko yeh bolna padega:

```
1. Ek pateela lo
2. Usme 1 cup paani daalo
3. Gas jalao
4. Pateela gas pe rakho
5. Jab tak paani ubalne na lage, intezaar karo
6. 1 chammach chai patti daalo
7. 2 minute intezaar karo
8. 1 cup doodh daalo
9. Ubalne do
10. Cheeni daalo
11. Chhan lo
12. Cup mein daalo
```

Yeh ek **algorithm** hai — step by step instructions.

Ab agar aap step 3 bhool jao? Paani thanda hi rahega. **Bug.**
Agar aap step 8 ko step 2 se pehle likh do? Doodh khaali pateele mein — galat result. **Bug.**
Agar aap step 5 mein "intezaar" ki condition galat likho ("jab tak paani thanda na ho jaye")?
**Infinite loop** — kabhi khatam nahi hoga.

**Computer bilkul aisa hi hai.** Woh literally wahi karega jo aap bologe. Woh aapka
"matlab" nahi samajhta, sirf aapke **shabd** samajhta hai.

---

## Do zaroori shabd

### Algorithm
Kisi problem ko solve karne ke steps. **Language se independent.**
Aap algorithm kagaz pe Hindi mein bhi likh sakte ho.

### Program
Wahi algorithm, kisi **programming language** mein likha hua, jo computer chala sake.

```
   Problem  ->  Algorithm  ->  Program  ->  Computer chalata hai
   (samasya)   (soch/steps)   (code)      (result)
```

**Sabse badi beginner galti:** seedha code likhna shuru kar dena, bina soche.
Pehle kagaz pe steps likho. Phir code karo. Yeh aadat aapko baaki 90% se aage rakhegi.

---

## Ek real example: sabse bada number dhoondo

**Problem:** 5 numbers diye hain: `12, 45, 7, 89, 23`. Sabse bada kaunsa hai?

**Aapka dimaag:** "89" — instantly. Aapne "dekh liya".

**Computer nahi dekh sakta.** Usko steps chahiye:

```
Algorithm:
1. Ek dabba banao jiska naam "sabseBada" ho
2. Pehla number (12) usme daal do        -> sabseBada = 12
3. Agla number (45) uthao
4. Kya 45 > sabseBada(12)?  Haan -> sabseBada = 45
5. Agla number (7) uthao
6. Kya 7 > sabseBada(45)?   Nahi -> kuch mat karo
7. Agla number (89) uthao
8. Kya 89 > sabseBada(45)?  Haan -> sabseBada = 89
9. Agla number (23) uthao
10. Kya 23 > sabseBada(89)? Nahi -> kuch mat karo
11. Aur number nahi bache
12. Answer: sabseBada = 89
```

Yeh algorithm hai. Aur guess kya? **Yeh exact algorithm** C++ mein aisa dikhega
(abhi samajhne ki koshish mat karo, bas dekh lo):

```cpp
int numbers[] = {12, 45, 7, 89, 23};   // 5 numbers ka group
int sabseBada = numbers[0];             // dabba banaya, pehla number daala

for (int i = 1; i < 5; i++) {           // baaki 4 numbers pe ghoomo
    if (numbers[i] > sabseBada) {       // kya yeh bada hai?
        sabseBada = numbers[i];         // haan -> dabbe mein daal do
    }
}
// ab sabseBada = 89
```

Dekho — **algorithm ke 12 steps, code ki 7 lines ban gaye.** Yahi to programming hai.

---

## Programming mein aap sirf 5 cheezein karte ho

Chahe aap "Hello World" likho ya poora HFT trading system — sab in 5 building blocks
se bana hota hai:

| # | Building block | Matlab | Kahan padhoge |
|---|---|---|---|
| 1 | **Data rakhna** | values ko naam dena, store karna | folder 03 |
| 2 | **Calculation** | jodna, ghatana, compare karna | folder 05 |
| 3 | **Decision** | "agar yeh, to woh" | folder 06 |
| 4 | **Repetition** | "yeh 100 baar karo" | folder 07 |
| 5 | **Reuse** | ek kaam ko naam dekar baar baar use karna | folder 08 |

Bas. Poori programming yahi hai. Baaki sab in paanch ka combination hai.

Yaad rakhna — jab aage jaake templates, atomics, lock-free queues confuse karein,
tab wapas yaad karna: **yeh sab bhi in 5 cheezon se bane hain.**

---

## Bug kya hai?

**Bug** = program mein galti. Program woh nahi kar raha jo aap chahte the.

Do type ke hote hain:

### 1. Compile-time error (syntax error)
Aapne language ke grammar rules tode. Compiler code ko samajh hi nahi paaya.
**Achhi baat hai** — compiler bata deta hai kahan galti hai.

```cpp
int x = 5      // <- semicolon nahi hai, compiler chillayega
```

### 2. Run-time / logic error
Code compile ho gaya, chal bhi gaya, par **galat answer** de raha hai.
**Yeh khatarnak hai** — compiler kuch nahi bolta, aapko khud dhoondhna padta hai.

```cpp
int average = a + b / 2;   // compile ho jayega
// par galat hai! sahi hai: (a + b) / 2
```

> Fun fact: "Bug" shabd 1947 se aaya, jab Grace Hopper ne Harvard Mark II computer se
> ek asli **moth (patanga)** nikala jo relay mein fas gaya tha. Woh literally computer
> ka pehla bug tha.

---

## Debugging kya hai?

Bug ko dhoondh kar theek karna. Ek programmer ka **50-70% time** debugging mein jaata hai.

Yeh normal hai. Yeh failure nahi hai. 20 saal ka experienced HFT engineer bhi roz debug
karta hai. Fark sirf itna hai ki woh **tezi se** debug karta hai, kyunki usne hazaaron
bugs dekhe hain.

Aapka goal: bugs se darna nahi, **unhe padhna seekhna.**

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Programmers ko sab syntax yaad hota hai" | Nahi. Sab documentation dekhte hain. Concepts yaad rakhne hote hain, syntax nahi |
| "Achhe programmer ka code first try mein chalta hai" | Kabhi nahi. Sab ke code mein errors aate hain |
| "Programming = math" | Zyada tar programming mein basic math hi lagti hai. Logic zyada lagta hai |
| "Mujhe genius hona padega" | Nahi. Patience aur practice chahiye. Bas |

---

## Exercises

1. **Kagaz pe** algorithm likho: "Ek number even hai ya odd, kaise pata karein?"

2. Algorithm likho: "3 numbers mein se sabse chhota kaunsa hai?"

3. Algorithm likho: "1 se 10 tak ke sabhi numbers ka sum."

4. Yeh chai wala algorithm hai. Isme **do bugs** hain. Dhoondho:
   ```
   1. Pateela lo
   2. Cheeni daalo
   3. Gas jalao
   4. Chai patti daalo
   5. Chhan lo
   6. Cup mein daalo
   ```
   <details><summary>Answer</summary>
   Bug 1: Paani/doodh daala hi nahi — sirf sookhi cheeni aur patti jal jayegi.
   Bug 2: Ubalne ka koi step nahi hai — chai bani hi nahi.
   Yeh "missing step" bugs hain — real code mein bahut common.
   </details>

5. Upar wale "sabse bada number" algorithm ko badal ke "sabse chhota number" ka bana do.
   Kya-kya badla?

6. Yeh algorithm kya karta hai? Trace karo (numbers: 3, 7, 2):
   ```
   total = 0
   har number ke liye:
       total = total + number
   answer = total
   ```
   <details><summary>Answer</summary>
   total=0 → total=0+3=3 → total=3+7=10 → total=10+2=12. Answer = 12. Yeh sum nikaalta hai.
   </details>

---

## Next
→ [`03-what-is-a-programming-language.md`](03-what-is-a-programming-language.md)
