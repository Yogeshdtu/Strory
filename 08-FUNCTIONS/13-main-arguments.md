# 13 — `main(int argc, char* argv[])` — command-line arguments

## Prerequisites
- [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md)
- `02-CPP-FIRST-STEPS/04-main-function.md`
- `04-INPUT-OUTPUT/` (input parsing, `std::from_chars`)

## Yeh topic abhi kyun
`./program hello 42 --verbose` — yeh 4 words program tak kaise pahunchte hain?
`main` ke arguments se. Har real CLI tool (compilers, `git`, aapke benchmarks)
isi se config leta hai. Aur `argv` ek **C-string array** hai — pointers ka pehla
practical parichay (folder 12).

---

## Do valid signatures

```cpp
int main() { ... }                       // arguments chahiye nahi
int main(int argc, char* argv[]) { ... } // command-line arguments
```

`char* argv[]` == `char** argv` (array parameter → pointer decay, folder 09).
Naam `argc`/`argv` convention hai — kuch bhi rakh sakte ho.

---

## `argc` aur `argv` ka structure

```
$ ./myapp  hello  42  --verbose

argc = 4

argv ──► [0] ──► "./myapp\0"
         [1] ──► "hello\0"
         [2] ──► "42\0"
         [3] ──► "--verbose\0"
         [4] ──► nullptr          ← array yahan KHATAM (guaranteed)
```

| | |
|---|---|
| `argc` | argument **count** — hamesha **≥ 1** |
| `argv[0]` | program ka naam / path (OS/shell decide karta hai — kabhi full path, kabhi bas naam) |
| `argv[1]` .. `argv[argc-1]` | user ke diye arguments, order mein |
| `argv[argc]` | **`nullptr`** — standard guarantee. Loop terminator ki tarah use kar sakte ho |

Har `argv[i]` ek **null-terminated C-string** hai. Shell ne quoting/globbing
pehle hi resolve kar diya — `"a b"` (quoted) ek argument, `*.txt` expand hoke kai.

---

## Padho — basic

```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "argc = " << argc << "\n";
    for (int i = 0; i < argc; ++i)
        std::cout << "argv[" << i << "] = " << argv[i] << "\n";

    // argv[argc] == nullptr use karke bhi:
    for (char** p = argv; *p != nullptr; ++p)
        std::cout << *p << "\n";
}
```

---

## Modern C++ — `std::span` / `string_view`

Raw `char**` se kaam karna error-prone. Wrap karo:

```cpp
#include <span>
#include <string_view>

int main(int argc, char* argv[]) {
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));

    for (std::string_view arg : args) {          // safe iteration
        std::cout << arg << "\n";
    }

    // argv[0] chhod ke sirf real args:
    for (std::string_view arg : args.subspan(1)) { ... }
}
```

`std::string_view` — no copy, `==` content compare, safe. Argument strings
process ke lifetime tak zinda rehti hain, to view safe hai.

---

## Parsing — flags aur values

```cpp
int main(int argc, char* argv[]) {
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));

    bool verbose = false;
    std::string_view inputFile;
    int threads = 1;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view a = args[i];

        if (a == "--verbose" || a == "-v") {
            verbose = true;
        } else if (a == "--threads") {
            if (i + 1 >= args.size()) { std::cerr << "--threads needs a value\n"; return 1; }
            std::string_view v = args[++i];
            if (auto n = toInt(v)) threads = *n;
            else { std::cerr << "bad --threads value\n"; return 1; }
        } else if (a == "--help" || a == "-h") {
            std::cout << "usage: " << args[0] << " [-v] [--threads N] FILE\n";
            return 0;
        } else if (a.starts_with("-")) {
            std::cerr << "unknown flag: " << a << "\n"; return 1;
        } else {
            inputFile = a;                       // positional argument
        }
    }
    // ...
}
```

### Number parsing — `std::from_chars` (no exceptions, no locale)

```cpp
#include <charconv>
#include <optional>

std::optional<long long> toInt(std::string_view s) {
    long long v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) return v;   // poora string consume hua?
    return std::nullopt;
}
```

`std::stoi` throws + partial-parse karta hai (`"42abc"` → `42`, no error).
`from_chars` fast, strict, exception-free — CLI parsing ke liye behtar.

**Real projects:** `CLI11`, `argparse`, `Boost.Program_options`, `getopt` — inhe
use karo bade tools ke liye.

---

## Return code — shell ise dekhta hai

```cpp
int main() {
    if (!ok) return 1;      // non-zero = failure
    return 0;               // 0 = success  (ya EXIT_SUCCESS / EXIT_FAILURE)
}
```

```bash
./myapp && echo "success"       # sirf tab jab return 0
./myapp; echo $?                # exact code (Linux); Windows: echo %ERRORLEVEL%
```

Convention: `0` = success, `1`+ = alag failure modes. Scripts / CI / `make` isi
pe chain karte hain.

`main` ke andar `return` optional hai (C++ implicitly `return 0;` karta hai agar
control end tak pahunche) — par explicit likhna behtar.

---

## `envp` — teesra (non-standard) parameter

```cpp
int main(int argc, char* argv[], char* envp[]) { ... }   // POSIX, standard nahi
```

Environment variables. Portable tareeka: `std::getenv("PATH")` (`<cstdlib>`).

---

## Andar kya hota hai

- OS/shell command line ko parse karta hai (quoting, globbing, variable
  expansion) → strings ka array banata hai
- Program load hote waqt loader `argc`, `argv` (aur `envp`) ko **stack pe**
  (main ke frame ke upar) rakhta hai
- C runtime startup (`_start` → `__libc_start_main`) `main(argc, argv)` call karta hai
- `argv` strings aur array **process ke lifetime tak** valid — inme `string_view`
  safe hai
- `main` return → runtime `exit(code)` → OS ko code milta hai

> **HFT relevance:** Trading systems apni config aksar files (YAML/JSON) ya
> environment se lete hain, CLI se sirf `--config path` aur `--dry-run` jaise
> switches. Startup path hot nahi hai — readability > cleverness. Par `argv`
> strings ka lifetime samajhna zaroori hai (zero-copy `string_view` config
> parsing). `main` ka return code CI/deploy scripts ke liye contract hai.

---

## Hands-on

`examples/07_command_line_args.cpp` — argc/argv print, `argv[argc] == nullptr`,
`--sum N N N` parsing, `--help`:

```bash
./build.ps1 08-FUNCTIONS/examples/07_command_line_args.cpp
# phir:
.build/07_command_line_args.exe hello world 42
.build/07_command_line_args.exe --sum 10 20 30 abc 5
.build/07_command_line_args.exe
```

---

## ⚠️ Traps

### Trap 1 — `argv[0]` ko hamesha program-naam samajhna
Kabhi full path, kabhi bas naam, kabhi (weird launchers) khali. Path chahiye to
OS API (`/proc/self/exe`, `GetModuleFileName`).

### Trap 2 — `argv[i]` ko modify karna aur assume karna safe hai
```cpp
argv[1][0] = 'X';       // technically allowed (non-const), par avoid -- copy le lo
```

### Trap 3 — count check bina `--flag value`
```cpp
if (a == "--out") outFile = args[i + 1];   // ⚠️ i+1 == argc -> OOB. Pehle check karo
```

### Trap 4 — `std::stoi` for CLI numbers
```cpp
int n = std::stoi("42abc");   // ⚠️ 42 -- no error. from_chars strict hai
int n = std::stoi("");        // 💥 throws std::invalid_argument
```

### Trap 5 — `int main(void)` / `void main()`
```cpp
void main() { }         // ❌ non-standard (kuch compilers accept karte hain)
int main() { }          // ✅
```

### Trap 6 — return code `> 255` (POSIX)
Shell exit codes `0..255` (8-bit). `return 300;` → `44` ban jaata hai POSIX pe.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`argc` mein sirf user args" | `argv[0]` (program) bhi count — `argc ≥ 1` |
| "`argv[0]` hamesha executable ka full path" | OS/shell decide — naam / path / khali |
| "`argv` array `argc` pe khatam" | `argv[argc] == nullptr` (guaranteed extra slot) |
| "`std::stoi` CLI numbers ke liye theek" | Partial-parse + throws — `from_chars` |
| "`void main()` valid hai" | Nahi — `int main()` ya `int main(int, char**)` |
| "return code kuch bhi ho sakta" | POSIX: 0..255 (8-bit truncate) |

---

## Exercises

1. **Echo:** `argc` aur har `argv[i]` print karo. `argv[argc]` `nullptr` hai —
   `char** p = argv; while (*p) { ...; ++p; }` se bhi print karo.

2. **Sum:** `./sum 3 4 5 10` → `22`. Non-number arg (`./sum 3 x 5`) → error message
   + skip ya `return 1`. `from_chars` use karo.

3. **Flags:** `-v`/`--verbose` (bool), `-n N`/`--count N` (int), aur ek positional
   filename parse karo. `--count` ke baad value missing → clean error.

4. **`std::span` wrapper:** `std::span<char*> args(argv, argc)` banao, `argv[0]`
   skip karke baaki `string_view` se iterate.

5. **Return codes:** ek program jo arg ke hisaab se `0` (`ok`), `1` (`fail`),
   `2` (`usage`) return kare. `./prog X; echo $?` se verify.

6. **`stoi` vs `from_chars`:** dono se `"42abc"`, `""`, `"999999999999"` parse
   karo. Behaviour difference note karo.

7. **Mini-grep:** `./mygrep PATTERN` — stdin se lines padho, jinme `PATTERN`
   (substring) ho unhe print. `argc != 2` → usage + `return 2`.

---

## Interview questions

1. `main` ke valid signatures? `argc` ki min value?
2. `argv[0]` mein kya hota hai — guaranteed?
3. `argv[argc]` kya hai? Kaise use kar sakte ho?
4. `argv` strings ka lifetime? `string_view` mein rakhna safe?
5. CLI number parsing — `std::stoi` vs `std::from_chars`, kaunsa better aur kyun?
6. `main` ka return code kaun dekhta hai? POSIX pe range?
7. `char* argv[]` aur `char** argv` — same hai? Kyun (decay)?

---

## Next
→ [`14-namespaces.md`](14-namespaces.md)
