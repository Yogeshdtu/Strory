# Glossary — har technical term ka Hinglish matlab

Jab bhi koi shabd samajh na aaye, yahan dekho. Yeh file course ke saath badhti rahegi.

Terms alphabetically nahi, **topic-wise** rakhe hain — taaki related cheezein saath dikhein.

---

## Basics

| Term | Hinglish matlab |
|---|---|
| **Program** | Instructions ka set jo computer follow karta hai |
| **Source code** | Woh text jo aap likhte ho (`.cpp` file mein) |
| **Compiler** | Program jo aapka source code machine code mein badalta hai |
| **Machine code** | 0 aur 1 mein likhe instructions, jo CPU seedha samajhta hai |
| **Executable** | Chalne wali file (`a.out`, `program.exe`) |
| **Binary** | Executable ka doosra naam; ya "0-1 wala number system" |
| **Syntax** | Language ke grammar rules |
| **Semantics** | Code ka matlab (kya karta hai) |
| **Statement** | Ek complete instruction, `;` pe khatam |
| **Expression** | Kuch jiski ek value hoti hai (`2 + 3`) |
| **Keyword** | Language ke reserved shabd (`int`, `return`, `if`) |
| **Identifier** | Aapka diya hua naam (variable/function ka naam) |
| **Literal** | Code mein seedha likhi hui value (`42`, `"hello"`, `3.14`) |
| **Token** | Code ka sabse chhota meaningful tukda |
| **Whitespace** | Space, tab, newline |

---

## Compilation

| Term | Hinglish matlab |
|---|---|
| **Preprocessor** | Compile se pehle text-level changes karta hai (`#include`, `#define`) |
| **Translation unit** | Ek `.cpp` file + uske sabhi includes, preprocess hone ke baad |
| **Object file** | `.o`/`.obj` — ek TU ka compiled output, abhi tak jud nahi hua |
| **Linker** | Object files ko jodkar executable banata hai |
| **Symbol** | Ek naam jo linker dhoondhta hai (function/variable) |
| **Name mangling** | C++ naamon ko unique bana deta hai (overloading ke liye) |
| **Header file** | `.h`/`.hpp` — declarations rakhne ki file |
| **Include guard** | Header ko do baar include hone se rokta hai |
| **ODR** | One Definition Rule — ek cheez ki sirf ek definition |
| **Static linking** | Library ko executable ke andar hi daal dena |
| **Dynamic linking** | Library ko run-time pe load karna (`.so`/`.dll`) |
| **ABI** | Application Binary Interface — compiled code ka contract |

---

## Memory

| Term | Hinglish matlab |
|---|---|
| **RAM** | Fast temporary memory; bijli jaate hi khali |
| **Address** | Memory ka "ghar ka number" |
| **Byte** | 8 bits; memory ki sabse chhoti addressable unit |
| **Word** | CPU ki natural size (64-bit machine pe 8 bytes) |
| **Stack** | Fast automatic memory; function calls ke liye |
| **Heap** | Manual/dynamic memory (`new`/`delete`) |
| **Static storage** | Poore program ke liye zinda memory |
| **Pointer** | Variable jo ek address store karta hai |
| **Reference** | Kisi existing object ka doosra naam (alias) |
| **Dereference** | Pointer ke andar ki value nikalna (`*p`) |
| **Dangling pointer** | Pointer jo mari hui memory pe point kar raha hai |
| **Memory leak** | Allocate kiya, free nahi kiya |
| **Alignment** | Address ka kisi number ka multiple hona |
| **Padding** | Alignment ke liye struct mein daale gaye khaali bytes |
| **Cache line** | Memory ka chunk jo CPU ek baar mein laata hai (usually 64 bytes) |

---

## Types & objects

| Term | Hinglish matlab |
|---|---|
| **Type** | Data ka kism — batata hai bits ka matlab kya hai |
| **Object** | Memory ka ek region jisme typed value hai |
| **Variable** | Naam wala object |
| **Declaration** | Compiler ko batana ki yeh naam exist karta hai |
| **Definition** | Actually banana (memory allocate hoti hai) |
| **Initialization** | Pehli baar value dena |
| **Assignment** | Existing object ki value badalna |
| **Scope** | Naam kahan tak visible hai |
| **Lifetime** | Object kab tak zinda hai |
| **Storage duration** | Automatic / static / dynamic / thread |
| **Narrowing** | Badi type se chhoti mein daalna (data loss ho sakta hai) |
| **Implicit conversion** | Compiler khud type badal deta hai |
| **UB (Undefined Behaviour)** | Standard ke hisaab se kuch bhi ho sakta hai — sabse khatarnaak cheez |

---

## OOP

| Term | Hinglish matlab |
|---|---|
| **Class** | Blueprint — object banane ka naksha |
| **Instance** | Class se bana actual object |
| **Member** | Class ke andar ka variable ya function |
| **Constructor** | Object banate waqt chalne wala function |
| **Destructor** | Object khatam hote waqt chalne wala function |
| **Encapsulation** | Data ko chhupana, interface dena |
| **Inheritance** | Ek class ka doosri se properties lena |
| **Polymorphism** | Ek interface, alag-alag behaviour |
| **Virtual function** | Run-time pe decide hone wala function call |
| **vtable** | Virtual functions ka pointer table |
| **vptr** | Object ke andar vtable ka pointer |
| **Abstract class** | Jisme koi pure virtual function ho; object nahi ban sakta |
| **Slicing** | Derived object ko base type mein copy karne pe extra data kat jaana |
| **RAII** | Constructor mein resource lo, destructor mein chhodo |

---

## Move semantics

| Term | Hinglish matlab |
|---|---|
| **lvalue** | Jiska address le sakte ho; naam wali cheez |
| **rvalue** | Temporary value, address nahi le sakte |
| **prvalue** | Pure rvalue — jaise `42`, `f()` ka return |
| **xvalue** | "Expiring value" — jiske resources churaye ja sakte hain |
| **Copy** | Data ki nayi copy banana |
| **Move** | Data ka ownership transfer karna (bina copy kiye) |
| **`std::move`** | Cast hai — object ko "move-able" mark karta hai. **Khud kuch move nahi karta** |
| **Rule of 3/5/0** | Kaunse special member functions likhne chahiye |
| **RVO/NRVO** | Compiler copy hata deta hai return ke waqt |
| **Perfect forwarding** | Argument ka exact type/category aage bhejna |

---

## Concurrency

| Term | Hinglish matlab |
|---|---|
| **Process** | Chalti hui program instance; apni alag memory |
| **Thread** | Process ke andar ek execution flow; memory share karta hai |
| **Concurrency** | Kai kaam "ek saath chalte dikhna" |
| **Parallelism** | Kai kaam sach mein ek saath (multiple cores) |
| **Race condition** | Result timing pe depend kar jaaye |
| **Data race** | Do threads ek hi memory pe, kam se kam ek write, bina sync = **UB** |
| **Critical section** | Code ka woh part jo ek time pe ek hi thread chala sake |
| **Mutex** | Taala — ek time pe ek thread |
| **Deadlock** | Do threads ek doosre ka intezaar, dono atke |
| **Atomic** | Operation jo beech mein toota nahi ja sakta |
| **CAS** | Compare-And-Swap — lock-free ka building block |
| **Memory ordering** | Rules ki operations kis order mein dikhein |
| **happens-before** | Formal guarantee ki A, B se pehle hua |
| **Lock-free** | Bina mutex ke, aur koi thread block nahi hota |
| **False sharing** | Alag variables ek hi cache line pe — performance kharab |

---

## Performance / HFT

| Term | Hinglish matlab |
|---|---|
| **Latency** | Ek kaam mein kitna time laga |
| **Throughput** | Per second kitne kaam nipte |
| **Jitter** | Latency ka up-down hona (inconsistency) |
| **Tail latency** | Sabse slow requests — p99, p99.9 |
| **p50 / p99** | Median / 99th percentile latency |
| **Cache miss** | Data cache mein nahi mila, RAM se laana pada (slow) |
| **Branch misprediction** | CPU ne galat andaza lagaya, pipeline flush |
| **Pipeline** | CPU instructions ko assembly-line jaisa process karta hai |
| **Out-of-order execution** | CPU instructions ko re-order karke chalata hai |
| **SIMD** | Ek instruction, multiple data — vector operations |
| **NUMA** | Multi-socket machines mein memory ki distance alag-alag |
| **Prefetch** | Data pehle se cache mein laana |
| **Memory pool** | Pehle se allocate ki hui memory, reuse ke liye |
| **Ring buffer** | Fixed-size circular buffer |
| **SPSC** | Single Producer Single Consumer queue |
| **Zero-copy** | Data ko copy kiye bina process karna |
| **Kernel bypass** | Network packets ko kernel se bachakar directly userspace mein |
| **Busy-spin** | Sona nahi, loop mein check karte rehna (latency kam, CPU zyada) |
| **Page fault** | Memory access jo kernel ko involve karta hai (slow) |
| **Syscall** | Kernel se kaam maangna (mehnga hota hai) |

---

## Trading / HFT domain

| Term | Hinglish matlab |
|---|---|
| **HFT** | High-Frequency Trading — bahut tez, bahut zyada trades |
| **Exchange** | Jahan buyers aur sellers milte hain (NSE, Nasdaq) |
| **Order** | Buy/sell karne ka instruction |
| **Limit order** | "Is price ya better pe hi karo" |
| **Market order** | "Jo bhi price ho, abhi karo" |
| **IOC** | Immediate-Or-Cancel — jitna abhi bhare, baaki cancel |
| **FOK** | Fill-Or-Kill — poora bhare ya kuch nahi |
| **Bid** | Sabse ooncha buy price |
| **Ask / Offer** | Sabse neecha sell price |
| **Spread** | Ask − Bid |
| **Order book** | Sabhi pending orders ka price-wise organized view |
| **Price level** | Ek price pe pade sab orders |
| **Price-time priority** | Behtar price pehle; same price pe pehle aane wala pehle |
| **Fill / Execution** | Order ka poora ya aadha match ho jaana |
| **Matching engine** | Exchange ka woh part jo orders match karta hai |
| **Market data** | Exchange se aane wala live price/order stream |
| **Feed handler** | Market data receive aur parse karne wala component |
| **L1 / L2 / L3** | Top-of-book / price levels / individual orders |
| **Tick** | Ek price update; ya minimum price movement |
| **ITCH** | Nasdaq ka binary market data protocol |
| **FIX** | Text-based order/trade messaging protocol |
| **SBE** | Simple Binary Encoding — fast binary protocol |
| **Co-location** | Exchange ke data center mein apna server rakhna |
| **Market maker** | Dono taraf quotes deta hai, spread se kamaata hai |
| **OMS** | Order Management System |
| **Tick-to-trade** | Market data aane se order jaane tak ka total time |
| **Wire-to-wire** | NIC pe packet aane se NIC se packet jaane tak ka time |

---

Koi term missing hai? Apne notes mein add kar lo — aur agle batch mein bata dena,
main isme daal dunga.
