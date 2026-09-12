# 08 — Shared memory: POSIX shm, cross-process rings

## Prerequisites
- `07-mmap.md` (MAP_SHARED, page cache)
- `27-ATOMICS-MEMORY-MODEL` + `28-LOCK-FREE` file 04 (SPSC ring)
- `03-processes.md` (processes = crash isolation)

## Yeh topic abhi kyun
Threads memory share karte — par ek thread ka crash poore process ko le doobta.
HFT mein feed handler, strategy, aur order gateway aksar **alag processes** hote
(ek crash → baaki zinda), phir bhi unhe thread jitni fast communication chahiye.
Jawab: **shared memory** + wahi lock-free rings jo folder `28` mein banaye. Yeh
lesson dono ko jodta hai.

---

## POSIX shared memory: `shm_open` + `mmap`

```cpp
#include <sys/mman.h>
#include <fcntl.h>

// --- Producer / creator ---
int fd = ::shm_open("/md_ring", O_CREAT | O_RDWR | O_EXCL, 0600);
::ftruncate(fd, sizeof(RingLayout));
void* base = ::mmap(nullptr, sizeof(RingLayout),
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
::close(fd);                                   // mapping fd ke bina zinda
auto* ring = new (base) RingLayout{};          // placement new -- ek baar

// --- Consumer (dusra process) ---
int fd2 = ::shm_open("/md_ring", O_RDWR, 0600);
void* base2 = ::mmap(nullptr, sizeof(RingLayout),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
::close(fd2);
auto* ring2 = static_cast<RingLayout*>(base2); // SAME physical RAM as ring

// --- Cleanup (koi ek, aakhir mein) ---
::munmap(base, sizeof(RingLayout));
::shm_unlink("/md_ring");                       // naam hataao
```

- Naam `/` se shuru — actually `/dev/shm/md_ring` (ek **tmpfs** file, RAM-backed).
- `shm_unlink` sirf **naam** hatata; jo processes ne map kiya hai woh tab tak
  zinda jab tak sab `munmap` na karein (fd/mapping refcount).
- Purani glibc pe link `-lrt`.

### Alternatives
| Mechanism | Note |
|---|---|
| **`shm_open` + `mmap`** | modern POSIX, fd-based, `/dev/shm` |
| **`mmap` on a real file** | persist chahiye to (`/hugepages/ring` hugetlbfs pe → huge pages!) |
| **`mmap(MAP_SHARED\|MAP_ANONYMOUS)` before `fork`** | parent+children share, no name |
| **System V `shmget`/`shmat`** | legacy; `ipcs`/`ipcrm`; key_t; avoid for new code |
| **`memfd_create` + fd passing** | anonymous, pass fd over Unix socket; no `/dev/shm` name |

---

## Cross-process = cross-thread (memory model ke liye)

Hardware cache coherence **process ko nahi jaanta** — woh physical addresses pe
kaam karta. Do processes jinke virtual addresses same physical page pe map hain,
unke liye `std::atomic` + `memory_order_release`/`acquire` bilkul waise kaam
karte jaise do threads ke beech (folder `27`).

To folder `28` ka SPSC ring **as-is** shared memory mein daal do:

```cpp
struct alignas(64) RingLayout {
    // producer writes head_, consumer writes tail_ -- alag cache lines
    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    alignas(64) Msg buf_[kCapacity];              // kCapacity power-of-two
};
static_assert(std::atomic<uint64_t>::is_always_lock_free);
```

`try_push` / `try_pop` folder `28` file 04 se identical — sirf ab `RingLayout`
`/dev/shm` mein hai aur do alag PID isse touch karte. **Example `05` yeh exact
demo hai** (fork + shared counter hand-off, monotonic-verify).

---

## ⚠️ Layout ke rules (shared memory-specific)

### Rule 1 — sirf trivially-copyable, self-contained data
Shared region mein **koi pointer nahi** jo ek process ke address space ko refer
kare. Doosra process usi virtual address pe map ho, zaroori nahi. Rules:
- `std::string`, `std::vector`, `std::map` — **NAHI** (internal heap pointers).
- Pointers ki jagah **offsets** (base se relative) ya fixed-size indices.
- `std::atomic<integral>` — OK. `std::mutex` — OK **only** with
  `PTHREAD_PROCESS_SHARED` attribute (warna UB across processes).

### Rule 2 — dono side same struct definition
Ek `layout.hpp` dono binaries share karein. Struct ka size/offsets/padding
match hona chahiye — same compiler flags, `#pragma pack` consistency,
`static_assert(sizeof(RingLayout) == EXPECTED)`. Version field rakho.

### Rule 3 — placement-new sirf creator, ek baar
Consumer ko `new (base) RingLayout{}` **nahi** karna — woh atomics ko 0 kar
dega, producer ka data mita dega. Creator ek baar construct kare (ek init flag
/ `O_EXCL` se decide), baaki `reinterpret_cast`/`static_cast`.

### Rule 4 — ABA aur reclamation: monotonic counters se aasan
SPSC ring ke `head_`/`tail_` **monotonically increasing** (kabhi wrap nahi
64-bit pe) → ABA structurally impossible (folder `28` file 04). Cross-process
mein yeh aur important — ek process crash ho jaye beech mein, counters
consistent snapshot dete hain.

---

## Crash handling — shared memory ka asli faayda aur khatra

**Faayda:** consumer crash → producer zinda, ring mein likhta rehta (jab tak
full na ho). Naya consumer restart hoke `tail_` se resume kar sakta.

**Khatra:** producer beech-write mein crash → `buf_[head_]` half-written, phir
`head_.store(release)` **nahi** hua → consumer use dekhta hi nahi (release store
ke bina publish nahi). Ring safe. Par agar tumne 2-step publish design kiya
(size likho, phir data) bina proper ordering ke → torn.

**`std::mutex` in shared memory + crash = permanent deadlock.** Agar lock
holder process mar gaya, lock hamesha ke liye held. `PTHREAD_MUTEX_ROBUST` +
`pthread_mutex_consistent` se recover karo, ya — behtar — **lock-free rings hi
use karo**, koi shared lock nahi.

---

## Internal working

- `/dev/shm` ek `tmpfs` mount — files RAM mein (swap pe ja sakti unless
  `mlock`). Size limit `df -h /dev/shm` (default ~half RAM).
- `mmap(MAP_SHARED)` dono processes ke page tables ko **same physical frames**
  pe point karwata. Pehli touch pe har process apne page table mein entry
  banata (minor fault), par frame shared.
- Coherence: producer core store karta → cache-coherence protocol (MESI) us
  line ko consumer ke core tak pahunchata — process boundary irrelevant.
  Cost = ek cache-line transfer (~10–80 ns, cores ki topology pe).
- NUMA: shared region jis node pe allocate hui, dusre node ka process usko
  "remote" access karega (`14`) — dono processes ko paas ke cores pe pin karo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `std::vector`/`std::string` ko shared struct mein daalna
Internal pointers dusre process mein garbage. Fixed arrays + lengths, ya
offset-based custom containers.

### Trap 2 — consumer bhi `placement new` kar deta
Producer ka state wipe. Sirf creator init kare (`O_EXCL` success = "main
creator hoon").

### Trap 3 — plain `std::mutex` cross-process
UB. `pthread_mutexattr_setpshared(PTHREAD_PROCESS_SHARED)` + robust attr, ya
lock-free.

### Trap 4 — `shm_unlink` bhool jaana
Har run ek stale `/dev/shm/name` chhod jaata → agli `shm_open(O_EXCL)` fail, ya
purana garbage map ho jaata. Startup pe `shm_unlink` (ignore ENOENT), ya cleanup
handler.

### Trap 5 — struct layout mismatch between binaries
Ek binary `-O2`, doosra `-O0`; ya alag struct version. `sizeof` / offsets
diverge → silent corruption. `static_assert(sizeof(...))` + magic + version
field, dono side check.

### Trap 6 — size = RAM se bada, ya `/dev/shm` limit se bada
`ftruncate` succeed, `mmap` succeed (lazy!), phir touch pe `SIGBUS` (tmpfs full)
ya OOM. `/dev/shm` size badhao (`mount -o remount,size=8G /dev/shm`) ya
hugetlbfs use karo.

### Trap 7 — not `mlock`ing the shared region
`/dev/shm` swappable hai. Trading hours mein shared ring swap-out → agli access
major fault (~ms). `mlock(base, size)` (ownership: creator).

---

## > **HFT relevance**

> - **Standard architecture:** feed handler (1 process) market data ko ek
>   shared-memory ring mein decode karke likhta; N strategy processes usi ring
>   se read (SPSC per consumer, ya ek MPSC-fanout). Ek strategy crash → feed +
>   baaki strategies chalti rehti.
> - **Order gateway** alag process — strategy shared ring mein order intents
>   daalti, gateway process unhe wire pe bhejta + risk checks. Isolation:
>   strategy bug se gateway/risk down nahi hote.
> - **`/dev/shm` pe hugetlbfs** — ring buffer huge pages pe → kam TLB miss, kam
>   page fault, aligned. `mmap("/dev/hugepages/ring", MAP_SHARED)`.
> - **No shared locks, ever.** Lock-free SPSC/MPSC rings only. Ek crashed
>   process kabhi kisi lock ko hold karke nahi mar sakta.
> - **Warm it:** creator `MAP_POPULATE` + `memset` + `mlock` shared region pe,
>   startup pe.

---

## Hands-on

```bash
# Linux pe -- example 05: fork + POSIX shm + monotonic hand-off verify
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/05_shared_memory.linux.cpp -o /tmp/shm -lrt && /tmp/shm

ls -l /dev/shm/                     # segment (agar cleanup se pehle dekho)
df -h /dev/shm                      # available size

# System V ka purana rasta bhi dekh lo (legacy):
ipcs -m                            # shared memory segments
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "cross-process ke liye special sync chahiye" | wahi `release`/`acquire` atomics — hardware process nahi jaanta |
| "`std::vector` shared struct mein daal do" | internal pointers invalid dusre process mein; fixed arrays/offsets |
| "`std::mutex` shared memory mein rakh do" | UB; `PROCESS_SHARED`+robust, ya lock-free |
| "`shm_unlink` = memory free" | sirf naam; mappers ke munmap pe free |
| "shared region swap nahi hogi" | `/dev/shm` tmpfs swappable; `mlock` karo |
| "dono binary alag build ho to chalega" | layout/size match zaroori; assert + version |

---

## Exercises

1. Producer process shared ring mein likh raha, consumer crash ho gaya. Producer
   ka kya hota? Naya consumer join kare to?

   <details><summary>Answer</summary>

   Producer chalta rehta jab tak ring full na ho jaaye (`head_ - tail_ ==
   capacity`), phir `try_push` fail hone lagega (backpressure). `tail_` shared
   memory mein hai, crash pe uska last value wahin. Naya consumer map karke
   `tail_` se resume kar sakta — beech ke messages jo full hone ke baad drop
   hue woh gaye, par ring corrupt nahi. (Isi liye SPSC ring cross-process
   robust — koi lock nahi jo crash pe stuck ho.)
   </details>

2. `RingLayout` mein `std::atomic<uint64_t> head_` aur turant `Msg buf_[N]`.
   `alignas` na ho to cross-process performance issue?

   <details><summary>Answer</summary>

   `head_` (producer likhta) aur `buf_[0]` (consumer padhta jab woh slot ready)
   agar same cache line mein → false sharing across cores (folder `28` file 03).
   Producer ka `head_` store consumer ke `buf_[0]` read ke line ko bounce
   karega. `alignas(64)` har hot field pe + trailing pad.
   </details>

3. Ek team shared struct mein `char symbol[8]` ki jagah `std::string symbol`
   rakhna chahti. Explain kyun toota.

   <details><summary>Answer</summary>

   `std::string` ke andar ek pointer (heap buffer), size, capacity hote (ya SSO
   buffer). Producer ke process mein woh pointer producer ke heap ko point
   karta — consumer process mein woh address ya to unmapped hai (`SIGSEGV`) ya
   kisi aur cheez pe (garbage read). Shared data self-contained hona chahiye:
   `char symbol[8]` (fixed) ya offset-into-a-shared-string-pool.
   </details>

4. `shm_open` `O_EXCL` ke saath — creator vs joiner kaise distinguish karein?

   <details><summary>Answer</summary>

   Creator: `shm_open("/name", O_CREAT | O_RDWR | O_EXCL, ...)` — success matlab
   "maine banaya, mujhe `ftruncate` + `placement new` + init karna hai". `EEXIST`
   mile → koi aur pehle bana chuka → phir `shm_open("/name", O_RDWR)` (bina
   `O_CREAT`) se join karo aur `reinterpret_cast`, koi init nahi. Ek `initialized`
   atomic flag ring mein rakho jo creator sabse aakhir mein set kare, joiner uspe
   spin kare.
   </details>

5. Cross-socket (NUMA) box: feed handler node 0, strategy node 1, shared ring
   node 0 pe allocate. Latency pe asar?

   <details><summary>Answer</summary>

   Strategy har ring access ke liye QPI/UPI interconnect ke paar jaayega
   (~remote memory latency, ~1.5–2× local, plus interconnect contention). Fix:
   dono processes ko **same NUMA node** ke cores pe pin karo, aur ring us node
   ki memory se allocate karo (`numactl --membind=0`, ya `mbind`). HFT: poora
   feed→strategy→gateway pipeline ek NUMA node pe rakho (`14`).
   </details>

---

## Interview questions

1. `shm_open` + `mmap` se shared memory setup — step by step.
2. `shm_unlink` kya karta, memory kab actually free hoti?
3. Cross-process atomics — kyun same `release`/`acquire` kaafi hai?
4. Shared struct mein kya nahi daal sakte, aur kyun (pointers)?
5. `std::mutex` shared memory mein — kya problem, alternatives?
6. Consumer/producer crash — SPSC ring kaise survive karta?
7. Threads vs processes+shm — HFT kyun aksar doosra chunta?

---

## Next
→ [`09-pipes-and-fifos.md`](09-pipes-and-fifos.md)
