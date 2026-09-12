# 18 — Behavioural round & discussing your projects

## Prerequisites
Your own work (this course's folders 39–44 count), a few real stories from
past jobs/projects.

## Why HFT firms run a behavioural round

Small teams, high trust, code hits production fast, mistakes cost real
money. They're checking: can you **communicate precisely**, do you
**own mistakes**, do you **handle being wrong**, are you **curious** and
**rigorous** (do you measure, do you dig), and will you be **easy to work
with under pressure**. It's usually 30–45 min, sometimes with an engineer
you'd work with.

---

## STAR — the structure for every story

**S**ituation (1–2 sentences of context) → **T**ask (what was your
specific responsibility) → **A**ction (what *you* did — "I", not "we",
with technical specifics) → **R**esult (outcome, ideally **measured**,
and what you learned).

Keep it ~2 minutes. Have 5–6 stories prepared that you can flex to
different prompts.

---

## The stories to prepare

### 1. "Tell me about a hard bug you fixed."
Pick one with a real diagnostic arc. Good shape:
- **S/T:** intermittent wrong output / latency spike / crash, hard to
  reproduce.
- **A:** how you made it reproducible (stress loop, seeded input,
  sanitizer), formed a hypothesis, used a specific tool (gdb watchpoint,
  ASan, TSan, `perf`, `rr`), found the **root cause** (not the symptom).
- **R:** the fix, a regression test, and the general lesson (e.g. "now I
  reach for a stress loop + sanitizer before touching code").

*From this course:* the crossed-book bug found between folders 43 and 44
(cached BBO only moved one direction, stale far orders never pulled) —
diagnosed by the strategy firing 0 signals, fixed in **both** the
simulator (non-crossing by construction) and the book (`resolve_cross`).
Or the 3-share correctness-gate divergence in folder 44's `FastVenue`
(aggregate sweep didn't touch per-order state → later cancels
over-subtracted) → fixed with a per-level FIFO. These are strong because
they show correctness-gate discipline and root-cause fixes.

### 2. "Tell me about a performance optimization."
- **S/T:** a stage was too slow / had a bad tail.
- **A:** baseline → profile (name the tool + what it showed) → one
  hypothesis with a *mechanism* → change → re-measure → explain.
- **R:** the measured before/after (ratios or numbers), and a caveat
  you're honest about.

*From this course:* the v0→v3 tick-to-order pipeline (folder 43): `std::map`
+ `std::stod` + `std::string` → flat array + hand integer parse +
fixed-point + division-free SMA → **~60–80×**, order-fire stream proven
byte-identical first. Mention the **honest Rule-2 null**: hot/cold path
splitting measured ~1% because the frontend wasn't the bottleneck — you
kept the attribute (cost 0) but reported the real result.

### 3. "A time you disagreed with a teammate / made the wrong call."
- Show you can **disagree respectfully, then commit** — or that you were
  wrong and **updated gracefully**.
- Ideal: a technical disagreement resolved by **measuring** ("we
  benchmarked both approaches; mine was slower; we shipped theirs").
- Avoid: villain stories, "I was right all along", or "we never resolved
  it."

### 4. "Something you built that you're proud of."
Your best project, STAR form, with the **measured** result and the
**trade-off you consciously made**. The CPP-MASTERY mini HFT engine works
here: a connected tick-to-trade pipeline, deterministic (replayable), with
the optimization loop applied end-to-end and a correctness gate proving
naive == optimized before any speedup claim.

### 5. "A time you didn't know something."
- What you didn't know, how you **figured it out** (docs, reading source,
  a small experiment, asking the right person), and what you know now.
- The signal: you're resourceful and not afraid to say "I don't know
  yet."

### 6. "Why HFT / why this firm?"
- Genuine technical reason: you like that correctness and latency are
  *measurable*, the feedback loop is tight, the systems are deep
  (kernel/network/cache all matter), small teams with ownership.
- Firm-specific: something real about their tech/market/culture — not
  "you're prestigious."
- Not: "the money." (True for everyone; says nothing.)

---

## Handling "I don't know" live

- **Say it plainly**, then add value: "I haven't used `io_uring` in
  production, but I understand it as a shared submission/completion ring
  that avoids per-op syscalls; I'd start by reading the man pages and
  writing a small echo server to measure it."
- Never bluff — the follow-up question will expose it, and now you've
  lost credibility on everything else.
- "Can I think about that for a moment?" is fine. So is "let me reason
  through it out loud."

---

## What NOT to say

- Badmouthing a current/former employer, manager, or teammate. Describe
  situations neutrally; focus on what *you* did.
- "We" for everything — the interviewer needs *your* contribution.
- Vague results — "it got faster" / "it worked better". Quantify or say
  you couldn't measure it and why.
- Claiming a speedup you never benchmarked (an HFT red flag specifically).
- "I don't really have questions." Have 3–5 (`01`).

---

## Discussing this course's projects credibly

You built them; own the details. Be ready for:

- "Walk me through your order book's data structure and its complexity."
  → flat array by tick, cached BBO, dense id index, O(1) add/cancel, BBO
  re-walk bounded by touch movement.
- "How did you know your optimization was correct?" → correctness gate:
  order-fire stream / fills / P&L / position proven byte-identical
  between naive and optimized across N seeds **before** reporting the
  speedup.
- "What would you do differently?" → have a real answer: more instruments
  / L3 book / a proper `perf` pass on Linux (the dev box was
  Windows/MinGW) / property-based tests.
- "What was the hardest part?" → the crossed-book bug, or making the
  whole pipeline deterministic (no wall clock anywhere).

Honesty about limitations ("benchmarked on an unpinned Windows box, so I
quote ratios not absolutes; tail numbers are jittery") is a **strength**
signal here.

---

## Quick prep checklist

- [ ] 6 STAR stories written out, ~2 min each, flexible to prompts
- [ ] One deep bug story with a real diagnostic arc + the tool used
- [ ] One optimization story with measured before/after + an honest caveat
- [ ] A crisp 60-second walkthrough of your best project
- [ ] A genuine, specific "why HFT / why here"
- [ ] 3–5 questions for them
- [ ] Practiced **out loud** (with `19`'s mock scripts)

## Next
→ [`19-mock-interviews.md`](19-mock-interviews.md)
