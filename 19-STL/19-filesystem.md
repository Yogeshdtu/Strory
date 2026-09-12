# 19 — `<filesystem>`: paths, directory iteration, file ops

## Prerequisites
- [`18-random.md`](18-random.md), folder 04 (I/O), folder 23 (error handling — this header does both throwing and error-code APIs)

## Yeh topic abhi kyun
Config files padhna, log directories scan karna, data files rotate karna,
capture files ka size check karna — ye sab `<filesystem>` (C++17). Isse pehle
log platform-specific code (`stat`, `opendir` / `FindFirstFile`) likhte the. Ab
ek portable API. HFT mein ye **startup/offline** ka tool hai, hot path ka nahi.

`#include <filesystem>`; `namespace fs = std::filesystem;`

---

## `fs::path` — a portable path

```cpp
namespace fs = std::filesystem;

fs::path p = "data/2026-08-30/quotes.csv";
p / "sub";                         // operator/ appends a component -> "data/2026-08-30/quotes.csv/sub"
p.parent_path();                   // "data/2026-08-30"
p.filename();                      // "quotes.csv"
p.stem();                          // "quotes"
p.extension();                     // ".csv"
p.replace_extension(".gz");        // in place
p.is_absolute(); p.is_relative();

for (const auto& part : p) { ... } // iterate components

fs::path abs = fs::absolute(p);
fs::path canon = fs::weakly_canonical(p);   // resolve . .. and symlinks (weakly: allows non-existent tail)
std::string s = p.string();                 // native narrow
```

- `path` just manipulates the *string* — it doesn't touch the disk. Separators
  are normalized to the platform; `operator/` is the safe way to join (don't
  `+ "/"`).
- On Windows a path is natively `wchar_t`; `p.string()` may throw on non-UTF-8
  content — prefer `p.u8string()` / `std::format` for display, and pass `path`
  objects around rather than strings.

## Queries — the disk-touching part

```cpp
fs::exists(p);
fs::is_regular_file(p);  fs::is_directory(p);  fs::is_symlink(p);
fs::file_size(p);                     // bytes (throws if not a regular file)
fs::last_write_time(p);              // fs::file_time_type
auto perms = fs::status(p).permissions();
fs::space("/").available;            // free bytes on the filesystem
```

## Directory iteration

```cpp
for (const auto& entry : fs::directory_iterator("logs")) {          // one level
    if (entry.is_regular_file() && entry.path().extension() == ".log")
        total += entry.file_size();                                  // cached in the entry -- no extra stat
}

for (const auto& entry : fs::recursive_directory_iterator("data")) { // whole tree
    if (entry.is_regular_file()) process(entry.path());
}
```

`directory_entry` **caches** `status`/`file_size`/`last_write_time` from the
single readdir — use `entry.file_size()`, not `fs::file_size(entry.path())`
(which re-stats).

## Mutating operations

```cpp
fs::create_directory("out");          fs::create_directories("a/b/c");   // mkdir -p
fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
fs::copy("srcdir", "dstdir", fs::copy_options::recursive);
fs::rename(from, to);                 // atomic within a filesystem; throws across filesystems
fs::remove(p);                        // one file / empty dir -> bool
fs::remove_all("tmp");                // recursive -> count removed
fs::resize_file(p, n);
fs::current_path();  fs::current_path("/new/cwd");
fs::temp_directory_path();
```

## Error handling — two API styles

```cpp
// (a) throwing -- fs::filesystem_error (carries the path(s) + the errno):
try { auto n = fs::file_size(p); }
catch (const fs::filesystem_error& e) { log(e.what(), e.path1(), e.code()); }

// (b) error_code overload -- no throw, check the code:
std::error_code ec;
auto n = fs::file_size(p, ec);
if (ec) { log("file_size failed:", ec.message()); }
```

**Every** filesystem function has both forms. Use the `error_code` form in loops
/ where a missing file is normal; the throwing form where failure is exceptional.

### TOCTOU

```cpp
if (fs::exists(p)) { auto f = open(p); ... }   // ⚠️ p can vanish between the check and the open
```
Checking then acting is a race. Prefer "just try the operation and handle the
error" (open it; if it fails, deal with it) over `exists()` gating.

---

## Andar kya hota hai

- `fs::path` stores one string (native encoding) plus lazily-computed component
  offsets. All the `parent_path`/`filename`/`extension` accessors are string
  scans — cheap, no I/O.
- The disk-touching calls wrap the OS: `stat`/`lstat`/`statx`, `opendir`/
  `readdir`, `mkdir`, `rename`, `unlink` on POSIX; `GetFileAttributesEx`,
  `FindFirstFileEx`, `CreateDirectory`, `MoveFileEx` on Windows. Each is a
  **syscall** — microseconds, not nanoseconds.
- `directory_iterator` holds an open directory handle and reads entries in
  batches; `recursive_directory_iterator` keeps a stack of them. `entry` carries
  whatever the readdir returned (on Linux, `d_type` often gives the file type
  with no extra `stat`).
- `fs::last_write_time` returns `file_time_type` (its own clock, `trivial_clock`
  / `file_clock`); converting to `system_clock` needs `clock_cast` (C++20) or a
  manual epoch adjustment.

> **HFT relevance:** `<filesystem>` belongs to **startup and offline** tooling —
> loading config and symbol files at boot, rotating/compressing yesterday's
> capture files, scanning a directory of pcaps for a backtest, checking free
> space before a recording run. It is **never** on the trading hot path: every
> non-`path` call is a syscall (µs-scale, can block on I/O). Hot-path logging
> writes to a pre-opened fd or an mmap'd ring, and the file rotation / naming is
> done by a background thread using this header. Prefer the `error_code`
> overloads in service code so a missing file doesn't throw across a real-time
> boundary.

---

## Hands-on

```cpp
// dirsize.cpp -- total size of *.log under a directory tree
#include <filesystem>
#include <cstdio>
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path root = argc > 1 ? argv[1] : ".";
    std::uintmax_t total = 0; int files = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file() && it->path().extension() == ".log") {
            total += it->file_size(); ++files;
        }
    }
    std::printf("%d .log files, %ju bytes\n", files, total);
}
```
```bash
g++ -std=c++20 -O2 dirsize.cpp -o dirsize && ./dirsize .
```

---

## ⚠️ Traps

### Trap 1 — building paths with string concatenation
```cpp
fs::path p = dir + "/" + name;   // ⚠️ wrong separator on Windows, double-slash bugs. dir / name
```

### Trap 2 — `fs::file_size` on a directory / missing file (throwing form)
```cpp
auto n = fs::file_size("somedir");   // ⚠️ throws filesystem_error. Guard with is_regular_file or use the ec overload
```

### Trap 3 — re-stat in a loop
```cpp
for (auto& e : fs::directory_iterator(d)) total += fs::file_size(e.path());   // ⚠️ extra stat per file. e.file_size() is cached
```

### Trap 4 — TOCTOU
```cpp
if (fs::exists(p)) fs::remove(p);   // ⚠️ race. fs::remove(p, ec); and ignore "not found"
```

### Trap 5 — `p.string()` on Windows with non-UTF-8 / wide content
```cpp
std::string s = widePath.string();   // ⚠️ can throw / lose data. Use u8string()/wstring(), or pass the path itself
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`fs::path` operations hit the disk" | Only string manipulation — `exists`/`file_size`/iteration touch the disk |
| "Join paths with `+ \"/\"`" | Use `operator/` — handles separators portably |
| "Filesystem calls are cheap" | Each is a syscall — µs scale, can block; keep off hot paths |
| "`exists()` then open is safe" | TOCTOU race — try the op and handle failure instead |
| "`last_write_time` is a `system_clock::time_point`" | It's `file_time_type` — `clock_cast` to convert (C++20) |

---

## Exercises

1. **Newest file:** find the most recently modified `*.csv` in a directory (one
   level), using the cached entry data.

   <details><summary>Answer</summary>

   `fs::path best; fs::file_time_type bt = fs::file_time_type::min(); for (auto&
   e : fs::directory_iterator(dir)) if (e.is_regular_file() &&
   e.path().extension() == ".csv") { auto t = e.last_write_time(); if (t > bt) {
   bt = t; best = e.path(); } }`
   </details>

2. **mkdir -p:** ensure `out/2026-08-30/session1/` exists, no error if it already
   does.

   <details><summary>Answer</summary>

   `std::error_code ec; fs::create_directories("out/2026-08-30/session1", ec); if
   (ec) log(ec.message());` — `create_directories` is already a no-op (returns
   false, no error) when the path exists.
   </details>

3. **Rotate:** rename `app.log` → `app.log.1` (and `app.log.1` → `app.log.2`
   first) before opening a fresh `app.log`. Sketch it.

   <details><summary>Answer</summary>

   From highest index down: `for (int i = keep-1; i >= 1; --i) { std::error_code
   ec; fs::rename(base + "." + std::to_string(i), base + "." + std::to_string(i+1),
   ec); }` then `fs::rename(base, base + ".1", ec);` then open `base` anew.
   Descending order so you don't overwrite a file you still need.
   </details>

4. **Free space guard:** before starting a capture that needs ~20 GB, bail if the
   target filesystem has less available.

   <details><summary>Answer</summary>

   `auto info = fs::space(targetDir); if (info.available < 20ull * 1024 * 1024 *
   1024) { log("insufficient space"); return; }`
   </details>

5. **error_code vs throw:** in a service that scans a directory every second,
   which API form for the per-file `file_size`, and why?

   <details><summary>Answer</summary>

   The `error_code` overload — a file disappearing between `readdir` and
   `file_size` is *normal* in a live directory; you want to skip it, not throw
   and unwind (potentially across a real-time boundary) once a second.
   </details>

---

## Interview questions

1. `fs::path` disk touch karta hai? Kaunse calls karte?
2. Paths join karne ka sahi tareeka — `operator/` kyun?
3. `<filesystem>` ki har function ke 2 forms — kaunsa kab?
4. TOCTOU kya, `exists()`-then-act kyun bura?
5. `directory_entry` caching — `e.file_size()` vs `fs::file_size(e.path())`?
6. HFT mein `<filesystem>` kahan use hota, hot path pe kyun nahi?

---

## Next
→ [`20-regex.md`](20-regex.md)
