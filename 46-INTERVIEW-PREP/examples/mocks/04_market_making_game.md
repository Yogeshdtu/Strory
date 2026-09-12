# Mock transcript — Market-making game + probability (~25 min)

> Market-maker style (Optiver / IMC / Jane Street / DRW). Do this fast
> and out loud. Composure > cleverness.

---

**Interviewer:** Make me a market on the sum of two fair dice.

**Candidate:** The expected value is 7. I'll quote **6.5 bid, 7.5 offer**
— I'll buy from you at 6.5 and sell to you at 7.5.

**Interviewer:** I lift your offer at 7.5.

**Candidate:** You bought from me at 7.5, so you might think it's worth
more than that, and I'm now **short**. I'll shift up and skew — I'm
keener to buy back than to sell more: **7.2 bid, 8.0 offer**.

**Interviewer:** I lift you again at 8.0.

**Candidate:** Another buyer — I keep moving up: **7.8 at 8.6**, and I'm
getting more cautious about the size I quote, because either you have
information or I'm accumulating a short I'll have to cover.

`[note: two-sided quotes, updates on the trade for BOTH information and
inventory, tightens/widens sensibly, stays calm.]`

**Interviewer:** Fine. Now: a fair coin, flipped until the first heads.
You win 2 to the power n, where n is the number of flips. What's a fair
price?

**Candidate:** The expected value is the sum over n of (1/2)^n times 2^n,
which is the sum of 1 forever — it diverges, infinite. But nobody pays a
huge finite amount for this, because of finite bankroll and because the
counterparty can't actually pay 2 to the 40th. So I'd price it by
capping the payout — if the max payout is a million dollars, the sum
becomes small and finite — or by utility rather than raw EV. It's the
St. Petersburg paradox; the point is not to blindly trust an infinite EV.

**Interviewer:** Expected number of flips to get two heads in a row?

**Candidate:** Let E be the answer. From the start: I flip once, that
costs 1. Half the time it's tails and I'm back to the start with E more
flips expected. Half the time it's heads and I'm in a "one head" state.
From "one head": flip once, cost 1; half the time heads and I'm done,
half the time tails and I'm back to the start.

So E = 1 + ½E + ½(1 + ½·0 + ½E). That's E = 1 + ½E + ½ + ¼E, so E = 1.5 +
¾E, so ¼E = 1.5, so **E = 6**.

And a nice contrast — the expected wait for heads-then-tails is only 4,
not 6. The overlap in "heads heads" is what makes it longer.

`[note: sets up states, solves cleanly, sanity-checks with the HT
contrast.]`

**Interviewer:** You've seen a coin flipped 10 times: 7 heads. Make a
market on the probability of heads.

**Candidate:** The naive estimate is 0.7, but 10 flips is very little
data, so I'd shrink toward 0.5. With a uniform prior the posterior mean
is (7 + 1) over (10 + 2), which is 8/12, about 0.67. I'll quote around
that with a **wide** spread — say **0.58 bid, 0.76 offer** — because my
uncertainty is high. With a few hundred flips I'd tighten the spread and
shrink less.

**Interviewer:** Last one. 17 times 24, in your head.

**Candidate:** 17 times 24 is 17 times 25 minus 17, which is 425 minus
17, so **408**.

---

## Rubric

| Dimension | Weak | This candidate |
|---|---|---|
| Making a market | a single number | two-sided quote + spread; updates on the trade for information *and* inventory |
| EV under a twist | "the EV is infinite, so infinite" | recognizes St. Petersburg; prices by bankroll cap / utility |
| Probability setup | freezes or guesses | states, recursion, solves, sanity-checks (HT = 4) |
| Bayesian instinct | reports 0.7 flat | shrinks toward the prior, widens the spread for small samples |
| Mental math | slow / wrong | fast, uses (a × 25 − a) decomposition |
| Composure | flustered when the market moves against them | calm, keeps re-quoting |

**Bar for a strong hire:** treats "make a market" as a repeated
inventory-plus-information game, not a guessing contest; sets up
probability problems with states/equations rather than pattern-matching;
does arithmetic fast without freezing; and stays composed when the
interviewer trades against them.
