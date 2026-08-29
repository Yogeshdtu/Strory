# 01 — Computer kya hai?

## Prerequisites
Koi nahi.

## Yeh topic abhi kyun
Kyunki hum computer ko **instructions dene** waale hain. Agar aapko yeh nahi pata ki
computer ke andar kya-kya hai, to aapko kabhi samajh nahi aayega ki aapka code kahan
chal raha hai aur kyun kabhi slow, kabhi fast hota hai.

Aur HFT mein — jahan **nanoseconds** matter karte hain — yeh samajh sabse zyada zaroori hai.

---

## Simple jawab

Computer ek machine hai jo teen kaam karti hai:

```
   INPUT  ------->  PROCESS  ------->  OUTPUT
  (data lo)       (kaam karo)       (result do)
```

Bas. Calculator se lekar HFT trading server tak — sab yahi karte hain.

---

## Computer ke 4 main hisse

```
  +-----------------------------------------------------+
  |                    COMPUTER                          |
  |                                                      |
  |   +-------------+          +--------------------+   |
  |   |     CPU     |  <---->  |   MEMORY (RAM)     |   |
  |   |  (dimaag)   |          |  (short-term yaad) |   |
  |   +-------------+          +--------------------+   |
  |          ^                          ^                |
  |          |                          |                |
  |          v                          v                |
  |   +---------------------------------------------+   |
  |   |        STORAGE (SSD / Hard Disk)            |   |
  |   |          (long-term yaad)                    |   |
  |   +---------------------------------------------+   |
  |                                                      |
  +------------------------ ^ --------------------------+
                            |
                +-----------+-----------+
                |    INPUT / OUTPUT      |
                | keyboard, screen,      |
                | mouse, network card    |
                +------------------------+
```

---

### 1. CPU — Central Processing Unit (dimaag)

CPU woh chip hai jo **actually kaam karti hai**. Yeh calculations karti hai, decisions
leti hai, aur instructions follow karti hai.

**Important baat:** CPU sirf bahut hi simple kaam kar sakti hai:
- do numbers jodo
- do numbers ghatao
- do numbers compare karo
- memory se value lao
- memory mein value rakho
- kisi doosri jagah jump karo

Bas! Itna hi. Aapka WhatsApp, aapka game, aapka trading system — sab in chhoti-chhoti
cheezon se bane hain, bas **karodon baar per second**.

**Speed:** Modern CPU ~3 GHz pe chalti hai = **3 arab (3,000,000,000) cycles per second**.
Ek cycle ~0.33 nanosecond. Ek nanosecond = 1 second ka arabva hissa.

> **HFT relevance (abhi bas note kar lo):** HFT systems mein hum poore trade decision ko
> kuch **microseconds** (1 µs = 1000 ns) mein karne ki koshish karte hain. Matlab kuch
> hazaar CPU cycles. Isliye har cycle count karta hai.

---

### 2. RAM — Random Access Memory (short-term yaad)

RAM woh memory hai jahan **abhi chal rahe programs** ka data rehta hai.

Analogy: RAM aapki **study table** hai. Jo kitaabein abhi padh rahe ho, wo table pe hain.
Table chhoti hai, par usse cheez uthana bahut fast hai.

**Do important properties:**
1. **Fast** — CPU seedha isse padh/likh sakti hai
2. **Volatile** — bijli gayi, sab kuch gaya. Permanent nahi hai.

Aaj ke laptops mein 8 GB – 32 GB RAM hoti hai. HFT servers mein 128 GB – 1 TB.

---

### 3. Storage — SSD / Hard Disk (long-term yaad)

Yahan aapki files permanently rehti hain — photos, videos, aur aapke `.cpp` files bhi.

Analogy: Storage aapki **almari/bookshelf** hai. Bahut saari kitaabein rakh sakte ho,
lekin nikalne mein time lagta hai.

| | RAM | Storage (SSD) |
|---|---|---|
| Speed | ~100 nanosecond | ~100 microsecond (1000x slow) |
| Size | 8–32 GB | 256 GB – 4 TB |
| Bijli jaane pe | sab gaya | sab bacha |
| Cost per GB | mehnga | sasta |

---

### 4. Input / Output devices

- **Input:** keyboard, mouse, microphone, **network card** (data andar aata hai)
- **Output:** screen, speaker, printer, **network card** (data bahar jaata hai)

> **HFT relevance:** HFT mein sabse important I/O device **network card (NIC)** hai.
> Market data NIC se aata hai, orders NIC se jaate hain. Poora game NIC se NIC tak ka
> time kam karne ka hai. Isko "wire-to-wire latency" kehte hain. Folder 42 mein detail.

---

## Ek aur cheez: OPERATING SYSTEM (OS)

Hardware ke upar ek software layer hoti hai — **Operating System**. Windows, macOS, Linux.

OS ka kaam:
- Programs ko chalana aur band karna
- Memory baantna (kis program ko kitni RAM)
- CPU ka time baantna (kaunsa program kab chale)
- Files manage karna
- Hardware se baat karna

```
  +----------------------------------+
  |   AAPKA PROGRAM (a.out)          |  <- aap yahan ho
  +----------------------------------+
  |   OPERATING SYSTEM (Linux)       |  <- yeh manager hai
  +----------------------------------+
  |   HARDWARE (CPU, RAM, NIC)       |  <- yeh actual machine hai
  +----------------------------------+
```

Aapka program **seedha hardware se baat nahi karta**. Woh OS se bolta hai: "mujhe file
kholni hai", "mujhe network se data chahiye". OS se kuch maangne ko **syscall** kehte hain.

> **HFT relevance:** Syscalls **slow** hote hain (~100–1000 nanoseconds+). HFT ka bada
> hissa yehi hai ki OS ko beech se hataya jaaye — isko "kernel bypass" kehte hain.
> Folder 29 aur 42 mein.

---

## Latency numbers — abhi sirf dekh lo

Yeh table abhi poora samajh nahi aayega. Bookmark kar lo. Course ke end tak yeh aapki
bible ban jayegi.

| Kaam | Time | Analogy (agar 1 CPU cycle = 1 second) |
|---|---|---|
| 1 CPU cycle | 0.3 ns | 1 second |
| L1 cache access | ~1 ns | 3 seconds |
| L2 cache access | ~4 ns | 13 seconds |
| L3 cache access | ~15 ns | 50 seconds |
| RAM access | ~100 ns | 5 minutes |
| SSD read | ~100 µs | 4 days |
| Hard disk seek | ~10 ms | 1 saal |
| Network (same datacenter) | ~500 µs | 20 din |
| Network (India → USA) | ~200 ms | 20 saal |

Dekha? **RAM se data laana CPU ke liye "5 minute ka intezaar" jaisa hai.** Isliye cache
itna important hai. Folder 32 mein poori kahani.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "CPU bahut smart hai" | CPU bahut **dumb** hai, bas bahut **tez** hai |
| "RAM aur storage same hai" | RAM temporary + fast, storage permanent + slow |
| "Zyada RAM = zyada speed" | Sirf tab jab RAM kam pad rahi ho |
| "Program CPU pe likha jaata hai" | Program storage mein hai, RAM mein load hota hai, CPU chalati hai |

---

## Exercises

1. Apne computer ki specs dekho:
   - Linux/Mac: `lscpu` (Linux), `sysctl -n machdep.cpu.brand_string` (Mac)
   - Windows: Task Manager → Performance
   Note karo: kitne cores? kitni RAM? CPU ki speed?

2. Linux/WSL pe chalao:
   ```bash
   cat /proc/cpuinfo | head -30
   free -h
   ```
   Kya dikha? CPU ka naam? RAM kitni?

3. Sochkar batao: aapka `.cpp` file storage mein hai ya RAM mein? Jab aap use compile
   karte ho to woh kahan jaata hai?
   <details><summary>Answer</summary>
   File storage (SSD) pe hai. Compile karte waqt compiler use RAM mein load karta hai,
   process karta hai, aur output (executable) wapas storage pe likhta hai. Jab aap
   program chalate ho, OS use storage se RAM mein load karta hai, phir CPU chalati hai.
   </details>

4. Upar wali latency table dekho. Agar aapko 1 million baar RAM se data laana pade
   (100 ns each), kitna total time lagega?
   <details><summary>Answer</summary>
   1,000,000 × 100 ns = 100,000,000 ns = 100 ms = 0.1 second.
   HFT ke liye yeh **eternity** hai. Isliye cache locality itna important hai.
   </details>

---

## Interview questions (abhi jawab nahi aayega, aage aayega)

1. RAM aur cache mein kya fark hai?
2. Syscall kya hai aur woh mehnga kyun hota hai?
3. Volatile memory ka matlab kya hai?

---

## Next
→ [`02-what-is-programming.md`](02-what-is-programming.md)
