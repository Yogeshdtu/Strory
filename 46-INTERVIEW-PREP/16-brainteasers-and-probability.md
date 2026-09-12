# 16 — Brainteasers & probability (Optiver / Jane Street style)

## Prerequisites
High-school probability + arithmetic fluency. Market-maker (Optiver, IMC,
Jane Street, DRW, Akuna, Flow) rounds lean heavily on this; latency/prop
shops less so, but EV reasoning still shows up.

## What they're actually testing

- **Expected value** thinking under uncertainty — fast, out loud, with a
  stated assumption when data is missing.
- **Mental math** — arithmetic without a calculator, at speed, without
  freezing.
- **Composure** — being wrong, catching it, and re-deriving calmly.
- **Market-making instinct** — quoting a two-sided price with a spread
  that reflects your uncertainty, and updating on information.

You don't need exotic math. You need to be *quick and unflappable* with
the basics.

---

## A — Expected value

### A1. I roll a fair die. You pay me $X to play; I pay you the number
shown. What's fair X?
<details><summary>Answer</summary>
E[roll] = (1+2+3+4+5+6)/6 = 3.5. Fair price **$3.50**. If you *had* to
make a market (bid/ask), you'd quote around it with a spread for your
uncertainty and their information — e.g. "3.30 / 3.70". If they can
choose to play only when favorable, widen. (`A5` on adverse selection.)
</details>

### A2. You roll a die; you may either take the value, or re-roll once and
take that. Strategy and EV?
<details><summary>Answer</summary>
Re-roll if the first roll is **below the EV of a fresh roll (3.5)** →
re-roll on 1, 2, 3; keep 4, 5, 6. EV = P(keep)·E[keep] + P(reroll)·3.5 =
(1/2)(5) + (1/2)(3.5) = **4.25**. (With two re-rolls allowed, the
threshold for the first roll becomes 4.25 → keep only 5, 6.)
</details>

### A3. A coin is flipped until the first heads. You win $2^n where n is
the number of flips. Fair price? (St. Petersburg)
<details><summary>Answer</summary>
E = Σ (1/2)^n · 2^n = Σ 1 = **infinite**. But nobody pays a huge finite
amount — because of finite bankroll, risk aversion, and the counterparty
can't actually pay $2^40. The interview point: recognize the paradox,
then price it by *utility* / a bankroll cap (e.g. "if the max payout is
capped at $1M, the sum is small"). Shows you don't blindly trust EV.
</details>

### A4. 3 doors, car behind one, you pick door 1, host opens door 3
(a goat), offers a switch. Switch?
<details><summary>Answer</summary>
**Switch** — P(win | switch) = 2/3, P(win | stay) = 1/3. Your original
pick was right 1/3 of the time; the host's constrained reveal (always a
goat, never your door) concentrates the other 2/3 on the remaining door.
Monty Hall. The key is that the host's action carries information.
</details>

### A5. Adverse selection in one line, and how it changes your quote.
<details><summary>Answer</summary>
If the counterparty gets to *choose* whether to trade with you, they
trade when it's good for them → bad for you. So you must **widen your
spread**: quote worse prices to compensate for the fact that fills are
selected against you. This is why a market maker's edge isn't "predict
the price" but "quote a spread that survives adverse selection."
</details>

---

## B — Making a market (the core game)

### B1. "Make me a market on the number of people in this building."
How do you respond?
<details><summary>Answer</summary>
(1) Estimate a midpoint out loud with a Fermi decomposition (floors ×
rooms × people/room, or headcount you can see × a multiplier). Say ~800.
(2) Quote a **two-sided** price around it with a spread that reflects
your uncertainty: **"750 at 850"** (you'll *buy* at 750, *sell* at 850).
(3) When the interviewer "trades" (hits your bid or lifts your offer),
**update**: a hit bid means the truth is probably lower → shift your
whole market down. Stay consistent, don't panic. The game tests
quoting, updating on order flow, and not getting picked off.
</details>

### B2. You quoted 750/850. Interviewer sells you 100 units at 750, then
asks for another market. What now?
<details><summary>Answer</summary>
They sold to you → they think it's worth less than 750 → the true value
is likely below your old mid, and you now hold a **long position** you
may need to exit. Move your market **down** (say 680/770) — lower mid
(new information) *and* skewed so you're keener to sell than buy (inventory
management). Explaining both effects — information and inventory — is the
strong answer.
</details>

### B3. Coin is possibly biased. You've seen 7 heads in 10 flips. Make a
market on P(heads).
<details><summary>Answer</summary>
Naive estimate 0.7, but 10 flips is little data — shrink toward 0.5
(a Bayesian prior: with a uniform prior, posterior mean = (7+1)/(10+2) =
**8/12 ≈ 0.67**). Quote around that with a wide spread, e.g. "0.58 /
0.76". More flips → tighter spread, less shrinkage. Shows you weight
evidence by sample size.
</details>

---

## C — Probability puzzles

### C1. Two kids, at least one is a boy. P(both boys)?
<details><summary>Answer</summary>
**1/3** — sample space {BB, BG, GB} (GG excluded), one of three is BB.
(If instead "the *older* is a boy", it's 1/2.) The wording of the
conditioning information matters — say which interpretation you're using.
</details>

### C2. 100 seats, 100 passengers. First passenger sits randomly; each
subsequent sits in their own seat if free, else randomly. P(last
passenger gets their own seat)?
<details><summary>Answer</summary>
**1/2.** By the time the last passenger boards, the only two seats that
can be left are passenger 1's seat or passenger 100's seat, and by
symmetry each is equally likely to be the one taken first. Clean
symmetry argument.
</details>

### C3. Expected number of coin flips to get two heads in a row (HH).
<details><summary>Answer</summary>
**6.** Let E = expected flips from start. From start: flip once (cost 1);
½ tails → back to start (E more), ½ heads → state "one H". From "one H":
flip once (cost 1); ½ heads → done, ½ tails → back to start.
E = 1 + ½E + ½(1 + ½·0 + ½E) → E = 1 + ½E + ½ + ¼E → E = 1.5 + ¾E →
¼E = 1.5 → **E = 6**. (For HT it's 4 — the difference surprises people.)
</details>

### C4. You have 25 horses, a race fits 5, no timer. Minimum races to
find the top 3?
<details><summary>Answer</summary>
**7.** 5 races of 5 (rank within each). 1 race of the 5 winners → its
winner is overall #1. Now only horses that can still be top-3: 2nd & 3rd
of the #1 group, 1st & 2nd of the #2 group, 1st of the #3 group = 5
horses. Race them (race 7) → top 2 of that race are overall #2 and #3.
</details>

### C5. Ants on a 1-meter stick, random positions and directions, speed
1 m/s, they reverse on collision. Longest time until all have fallen
off?
<details><summary>Answer</summary>
**1 second.** Two ants colliding and reversing is *indistinguishable*
from them passing through each other (relabel them). So treat every ant
as walking straight; the worst case is an ant starting at one end
heading to the other = 1 s.
</details>

---

## D — Estimation (Fermi)

### D1. How many golf balls fit in a school bus?
<details><summary>Answer</summary>
Bus interior ≈ 2.5 m × 2.5 m × 12 m ≈ 75 m³. Golf ball ≈ 4 cm diameter →
~65 cm³, but sphere packing wastes ~35%, and seats take ~30% of volume →
usable ~50 m³ / (65 cm³ / 0.65 packing) ≈ 50,000,000 cm³ / 100 cm³ ≈
**~500,000**. The number isn't the point — the *decomposition*,
*stated assumptions*, and *unit discipline* are.
</details>

### D2. How many trades per day on a large equity exchange?
<details><summary>Answer</summary>
~Thousands of symbols actively traded × ~hundreds-to-thousands of trades
each per day → **~10⁷–10⁸** trades/day, concentrated in the most liquid
names. Cross-check: a busy stock might trade every ~100 ms over a 6.5 h
session ≈ 200k trades; × a few thousand active names ≈ 10⁸. State the
range and your cross-check.
</details>

---

## E — Mental math drills (do these daily if MM)

- **Multiplication:** 17 × 24, 38 × 42, 76 × 76 (use (a±b)² and
  difference-of-squares: 76² = 80·72 + 16 = 5776).
- **Percentages of odd numbers:** 37% of 80, 15% of 240, 1/7 as a
  decimal (0.142857…).
- **Fractions ↔ decimals:** 3/8, 5/6, 7/16 instantly.
- **Sequences:** sum 1..n = n(n+1)/2; sum of first n odds = n².
- **The "80 in 8"-style test** (Optiver): ~80 arithmetic questions in 8
  minutes — practice at [that pace], accuracy first then speed.
- **Approximate then refine:** "17 × 24 ≈ 17 × 25 = 425, minus 17 = 408."

---

## F — Betting / sizing intuition (Kelly)

### F1. You have edge on a bet — how much of your bankroll do you wager?
<details><summary>Answer</summary>
**Kelly fraction**: for a bet paying `b` to 1 with win probability `p`,
`f* = p − (1−p)/b` (fraction of bankroll). It maximizes long-run
log-growth. In practice traders bet a **fraction of Kelly** (half or
less) because edge estimates are uncertain and full Kelly is very
volatile. Over-betting (past full Kelly) *reduces* growth and risks ruin.
The interview point: bet **more with more edge and more certainty, less
when unsure**, and never so much that a bad run wipes you out.
</details>

### F2. Two games: (A) 50% to double your money, 50% to lose it all;
(B) 50% +20%, 50% −20%, repeated many times. Which compounds?
<details><summary>Answer</summary>
(A) EV per play is +0 (1.0), but you go bust the first time you lose —
**terrible** to repeat. (B) EV of a round: ½(1.2) + ½(0.8) = 1.0, but
the *geometric* mean is √(1.2 × 0.8) = √0.96 ≈ **0.98 < 1** → you slowly
lose. Both have zero arithmetic edge but negative *geometric* growth when
repeated with full stake — volatility drag. This is why sizing (F1)
matters more than raw EV.
</details>

---

## Interview tips

- **Say your assumptions.** "I'll assume the die is fair / the building
  has 6 floors." A stated wrong assumption is fine; an unstated one
  isn't.
- **Estimate first, refine second.** Get a number out fast, then improve
  it.
- **When you make a market, quote two-sided and update on their trade** —
  the game is inventory + information, not a single guess.
- **Being wrong is fine — freezing isn't.** "Hmm, that's not right, let
  me redo it" is a *good* signal.
- **Don't over-precise.** "~500,000" beats "512,340" for a Fermi
  question.

## Next
→ [`17-trick-questions.md`](17-trick-questions.md)
