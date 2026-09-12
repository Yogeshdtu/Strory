# 01 — HFT interviews kaise kaam karte hain

## Prerequisites
Ideally folders 01–45 (yeh folder unhe **test** karta hai). Kam se kam:
`37-HFT-FUNDAMENTALS` (domain), `35-PROFILING` + `43-HFT-OPTIMIZATION`
(latency mindset).

## Yeh folder kyun

**Spec ka rule: HFT interviews pe seedha jump mat karo. Build toward them.**
Isliye `02`–`14` **layered** question banks hain — Layer 1 (types/loops)
se Layer 13 (full tick-to-trade design) tak. Har layer pe khud ko test
karo; jahan atko, wahi course folder dobara kholo.

Yeh lesson: process kaisa dikhta hai, kaun kya poochta hai, aur kaise
prepare karo.

---

## Firm types — kaun kya dhoondhta hai

| Type | Examples | Interview emphasis |
|---|---|---|
| **Market makers** | Optiver, IMC, Jane Street, DRW, Jump, Citadel Securities, Tower, HRT, Akuna, Flow | Probability + mental math + market-making games **heavy**; C++ solid but not always deepest; trading intuition |
| **Latency/prop shops** | Hudson River, Headlands, Radix, XTX, Quadeye, Tower (some desks) | C++ **deep**, systems/kernel/network, cache & branch prediction, "optimize this" live |
| **Quant-driven** | Two Sigma, Citadel (main), D.E. Shaw | Algorithms + stats + C++/Python; less pure-latency |
| **Exchanges / infra** | NSE/BSE tech, CME, LMAX, exchange-tech vendors | Systems design, matching-engine correctness, throughput, determinism |
| **Banks' e-trading** | low-latency desks at big banks | C++ + FIX + risk; slower-paced than pure HFT |

Tumhara CPP-MASTERY background (order book, matching engine, lock-free
queue, low-latency pipeline) sabse zyada **latency/prop** aur **exchange
infra** roles se match karta. Market-maker roles ke liye `16`
(brainteasers/probability) pe extra kaam karo.

---

## Typical pipeline

```
1. Recruiter screen          15–30 min   fit, comp, visa, timeline, motivation
2. Online assessment (OA)    60–120 min  HackerRank/Codility: 2–4 algo problems,
                                         sometimes C++-specific or a math section
3. Technical phone 1         45–60 min   C++ Q&A + 1 coding problem (shared editor)
4. Technical phone 2         45–60 min   systems / latency / concurrency deep-dive
5. Onsite / "superday"       half–full day
     - C++ deep-dive round
     - System design round (feed handler / order book / matching engine)
     - "Optimize this code" round (live profiling mindset)
     - Brainteaser / probability / market-making round (MM firms)
     - Behavioural + project discussion
     - Sometimes: a take-home (build a small order book / parser)
6. Offer + team matching
```

Timelines: OA → onsite typically 1–3 weeks; whole loop 3–8 weeks. Prop
shops move fast (sometimes offer in days). Keep pipelines parallel.

---

## The 3 axes they grade

Har round in teen mein se ek-do pe score karta:

1. **C++ / language depth** — not trivia; do you understand the *model*
   (object lifetime, value categories, the memory model, what the
   compiler is allowed to do). "Why is this UB and what could go wrong at
   `-O2`" > "recite the rule of five".
2. **Systems / latency intuition** — where do the nanoseconds and
   microseconds go? cache miss (~100 ns), branch mispredict (~5 ns),
   syscall (~1 µs), page fault, context switch, network hop. Can you
   reason about a latency budget (`37/14`)?
3. **Problem solving / communication** — do you *think out loud*, state
   assumptions, consider edge cases, and correct yourself gracefully?
   A wrong first answer that you catch beats a lucky right one.

MM firms add a 4th: **probability + EV + risk under uncertainty**.

---

## How to talk out loud (this is graded)

- **Restate the question.** "So you want a fixed-capacity queue, single
  producer single consumer, and I should optimize for latency not
  throughput — correct?"
- **State assumptions.** "I'll assume prices fit in `int64` ticks and the
  book has a bounded price range so I can use a flat array."
- **Give the naive answer first, then improve.** Interviewer wants to see
  the *progression* (this is literally folder 43's loop). Jumping to the
  clever answer without the baseline reads as memorized.
- **Name the complexity / the cost.** "This is O(1) amortized, one cache
  line touched, no allocation in the hot path."
- **When stuck:** say what you *do* know, what you'd try next, what you'd
  measure. Silence is the worst answer; bluffing is second worst.
- **When wrong:** "Actually wait — that's not right, because... let me
  redo it." Self-correction is a strong signal.

---

## Prep strategy (with this repo)

| Weeks out | Focus |
|---|---|
| 8+ | Fill gaps: run every `examples/` folder, do the `NN-exercises.md`. Weak areas → re-read that folder. |
| 4–6 | This folder's Layers 1–13 (`02`–`14`), 30 min/day. `47-CODING-PROBLEMS` graded sets. |
| 2–4 | System design (`15`), mock interviews (`19`), "optimize this" under a timer. Brainteasers (`16`) daily if MM. |
| 1 | `17` trick questions, `18` behavioural stories written out, `20` resume final, re-skim `48-CHEATSHEETS`. |
| Day before | Light review, sleep. Not new material. |

**Do real timed practice.** Reading answers ≠ producing them under
pressure with someone watching. Use `19`'s mock scripts with a friend.

---

## Red flags interviewers watch for

- Can't explain *why*, only *what* ("virtual destructor kyun?" → "rule
  hai" ❌ vs "base pointer se delete karne pe derived dtor chale, warna
  leak / UB" ✅).
- Never mentions **measuring** — claims speedups with no benchmark
  (violates the whole `43` ethos).
- Doesn't consider concurrency / lifetime / edge cases unprompted.
- Over-engineers a simple question; can't give the simple answer.
- Memorized-sounding answers that fall apart on a follow-up.
- Argues with the interviewer instead of exploring their hint.

---

## Questions to ask them (you should have 3–5)

- "What does the hot path look like — feed to order — and where's the
  current latency bottleneck?"
- "How do you measure and regression-test latency? p50 or p99.9?"
- "How much of the stack is C++ vs FPGA vs kernel-bypass?"
- "What does onboarding look like — how soon does new code hit prod?"
- "How is the team split — research vs core engineering vs infra?"
- (Not in round 1:) comp structure, bonus basis, PnL attribution.

---

## ⚠️ Common mistakes

### Mistake 1 — HFT interview ko LeetCode grind samajhna
Algo problems ek gate hain, focus nahi. Real weight: C++ model depth,
latency reasoning, design, (MM) probability. `47` ke problems karo, par
`02`–`14` + `15` + `16` pe zyada waqt.

### Mistake 2 — "I don't know" se darna
Har sawaal ka jawab nahi aata — expected. "I haven't worked with that
directly; here's how I'd reason about it / what I'd measure" is a fine
answer. Bluffing gets caught on the first follow-up.

### Mistake 3 — Projects ko explain na kar paana
Tumne is course mein ek order book, matching engine, lock-free queue,
aur ek mini HFT engine banaya. `18` + `20` mein inhe crisp story mein
dhaalo — problem, approach, *measured* result, kya seekha.

### Mistake 4 — Sirf padhna, bolna nahi
Verbal fluency alag skill hai. Mock karo (`19`), out loud.

---

## Next
→ [`02-cpp-basics-questions.md`](02-cpp-basics-questions.md)
