# 49 — END-TO-END PROJECTS

## Prerequisites
Varies by project — har project apni list deta hai (usually the matching course
folders).

## Yeh folder kyun
Spec ka rule: "hazaaron disconnected examples mat do — concepts ko **projects**
mein jodo."

3 tiers, difficulty order mein. Har project: **spec → milestones (v1 → v2 → v3)
→ kaunse concepts → tests → extensions.** Pehle khud banao, phir `examples/` ka
tested reference dekho.

## Is folder ki files

| # | File | Projects |
|---|------|----------|
| 01 | `01-beginner-projects.md` | calculator · number-guess · student manager · expense tracker · word stats · config parser |
| 02 | `02-intermediate-projects.md` | inventory · bank simulation · CSV parser+query · log analyzer · persistent KV store |
| 03 | `03-advanced-projects.md` | custom `Vector<T>` · allocators (arena/pool/segregated) · thread pool · concurrent queues (mutex/SPSC/MPSC) · epoll server · JSON parser |
| 04 | `04-project-guidelines.md` | v1-first approach · structure · error handling · testing (invariant/property/golden) · measuring · **code-review checklist** |
| 05 | `05-hft-projects-link.md` | HFT project track = folder 44; how these projects feed it |

## Examples

`examples/{beginner,intermediate,advanced}/` — **17 reference implementations**,
one per project, each a complete assertion-tested program with a TALKING POINTS
footer. `examples/README.md` mein mapping + how to run.

```bash
./build.ps1 checkall     # recursive -- compiles all 16 non-linux examples
g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 examples/beginner/01_calculator.cpp -o t && ./t
```

(`build.ps1 folder 49-PROJECTS` doesn't apply — examples are in subdirs.
`advanced/05_epoll_server.linux.cpp` is Linux-only → skipped by `checkall`.)

## Time
Ongoing — pick projects that match where you are in the course.

## Status
✅ **COMPLETE** — 5 lesson files (17 project specs + a guidelines/checklist doc)
+ 17 verified reference implementations. Mojibake 0.

## Next
→ [`../44-HFT-PROJECTS/00-README.md`](../44-HFT-PROJECTS/00-README.md)
