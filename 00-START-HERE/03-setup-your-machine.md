# Apna computer setup karo

## Prerequisites
Ek computer. Bas.

## Yeh topic abhi kyun

Kyunki C++ padhna aur C++ **chalana** — do alag cheezein hain. Bina compiler ke aap sirf
padhoge, seekhoge nahi. Yeh file 20-30 minute legi, ek baar. Uske baad zindagi bhar kaam
aayegi.

---

## Aapko exactly 3 cheezein chahiye

```
   1. COMPILER          2. EDITOR              3. TERMINAL
   (code ko exe        (code likhne ke       (commands chalane
    banata hai)          liye)                  ke liye)

   g++ / clang++        VS Code               bash / PowerShell
```

Bas. Aur kuch nahi.

---

## Option A: ONLINE (0 minute setup)

Agar aap abhi turant shuru karna chahte ho, bina kuch install kiye:

| Site | Kaam |
|------|------|
| https://godbolt.org | Compiler Explorer — code + assembly dono dikhata hai |
| https://www.programiz.com/cpp-programming/online-compiler/ | Simple online compiler |
| https://replit.com | Full online IDE |

**Lekin yeh sirf temporary solution hai.** Folder 24 (Compilation & Linking) se aage
aapko apni local machine chahiye hi chahiye. Aur folder 29 (Linux) se to Linux hi chahiye.

To behtar hai abhi hi proper setup kar lo.

---

## Option B: LOCAL SETUP (recommended)

### 🪟 WINDOWS

Windows pe do raaste hain. **Raasta 2 (WSL) strongly recommended hai** kyunki poora HFT
industry Linux pe chalta hai.

#### Raasta 1: MSYS2 (native Windows)

1. https://www.msys2.org se installer download karo, install karo
2. "MSYS2 UCRT64" terminal kholo (Start menu mein milega)
3. Yeh command chalao:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb
   ```
4. PATH mein `C:\msys64\ucrt64\bin` add karo
   (Start → "environment variables" search karo → Path → Edit → New)
5. Naya PowerShell kholo aur check karo:
   ```powershell
   g++ --version
   ```

#### Raasta 2: WSL2 — Windows pe asli Linux ⭐ RECOMMENDED

1. PowerShell ko **Administrator** ke roop mein kholo
2. ```powershell
   wsl --install
   ```
3. Computer restart karo
4. Ubuntu apne aap khul jayega, username/password set karo
5. Ubuntu terminal mein:
   ```bash
   sudo apt update
   sudo apt install build-essential gdb valgrind git cmake -y
   ```

Ab aapke paas Windows ke andar poora Linux hai. HFT ke liye yahi chahiye.

---

### 🍎 macOS

1. Terminal kholo (Cmd+Space → "Terminal")
2. ```bash
   xcode-select --install
   ```
   Ek popup aayega → "Install" dabao → 10-15 min lagenge
3. Check karo:
   ```bash
   clang++ --version
   ```

Behtar ke liye Homebrew se asli GCC bhi install kar lo:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install gcc cmake
```

**Note:** macOS pe `g++` actually `clang++` ka alias hota hai. Homebrew wala asli GCC
`g++-14` (ya jo bhi version ho) naam se milega.

**Ek aur note:** macOS pe `perf`, `numactl` jaisi Linux tools nahi hain. Folder 31-35
(performance engineering) ke liye aapko Linux chahiye hoga — VM, cloud, ya dual boot.

---

### 🐧 LINUX (sabse aasan — aur HFT ka ghar)

**Ubuntu / Debian / WSL:**
```bash
sudo apt update
sudo apt install build-essential gdb valgrind git cmake -y
```

**Fedora / RHEL:**
```bash
sudo dnf install gcc-c++ gdb valgrind git cmake -y
```

**Arch:**
```bash
sudo pacman -S base-devel gdb valgrind git cmake
```

Baad mein (folder 31+ ke liye) yeh bhi chahiye honge:
```bash
sudo apt install linux-tools-common linux-tools-generic numactl hwloc -y
```

---

## Editor: VS Code (recommended)

1. https://code.visualstudio.com se download karo, install karo
2. VS Code kholo → left sidebar mein Extensions icon (chaar dabbe) → yeh install karo:
   - **C/C++** (Microsoft) — IntelliSense, debugging
   - **C/C++ Extension Pack** (Microsoft)
   - **CodeLLDB** ya **C/C++ Themes** (optional)
   - **WSL** (agar Windows + WSL use kar rahe ho) — **zaroori hai**

3. **Important settings** — `Ctrl+,` dabao, search karo:
   - "C_Cpp: Default: Cpp Standard" → `c++20` set karo
   - "Files: Auto Save" → `afterDelay`

### Doosre editors (agar VS Code pasand na aaye)
- **CLion** (JetBrains) — powerful, paid (students ke liye free)
- **Vim / Neovim** — terminal-based, seekhne mein time lagta hai, HFT devs mein popular
- **Sublime Text**, **Notepad++** — halke

Abhi shuru mein VS Code sabse aasan hai. Baad mein jo pasand aaye.

---

## Terminal — 10 commands jo aapko abhi aane chahiye

Terminal ek text box hai jahan aap computer ko **likhkar** commands dete ho,
mouse se click karne ki jagah.

| Command | Kya karta hai | Example |
|---------|---------------|---------|
| `pwd` | abhi kaunse folder mein ho | `pwd` |
| `ls` | is folder mein kya kya hai | `ls` |
| `ls -la` | sab kuch, details ke saath | `ls -la` |
| `cd folder` | folder ke andar jao | `cd Desktop` |
| `cd ..` | ek folder peeche jao | `cd ..` |
| `cd ~` | home folder pe jao | `cd ~` |
| `mkdir naam` | naya folder banao | `mkdir cpp-practice` |
| `touch file` | khali file banao | `touch hello.cpp` |
| `cat file` | file ka content dikhao | `cat hello.cpp` |
| `rm file` | file delete karo (⚠️ hamesha ke liye) | `rm old.cpp` |
| `clear` | screen saaf karo | `clear` |

Windows PowerShell mein zyada tar yehi chalte hain. `ls`, `cd`, `pwd`, `mkdir` sab kaam
karenge.

**Tip:** `Tab` key dabao — terminal naam apne aap complete kar dega. `he` likh ke Tab
dabao → `hello.cpp` ban jayega. Yeh sabse useful shortcut hai.

**Tip 2:** Up-arrow ↑ dabao — pichli command wapas aa jayegi. Bar bar type mat karo.

---

## Sab kuch check karo — test drive

Terminal kholo aur exactly yeh karo:

```bash
# 1. Home folder pe jao
cd ~

# 2. Ek practice folder banao
mkdir cpp-practice

# 3. Uske andar jao
cd cpp-practice

# 4. Ek file banao (Linux/Mac)
cat > test.cpp << 'END'
#include <iostream>
int main() {
    std::cout << "Setup ho gaya! Compiler kaam kar raha hai.\n";
    return 0;
}
END

# 5. Compile karo
g++ -std=c++20 -Wall -Wextra -g test.cpp -o test

# 6. Chalao
./test          # Linux / macOS / WSL
# .\test.exe    # Windows PowerShell (native)
```

**Expected output:**
```
Setup ho gaya! Compiler kaam kar raha hai.
```

Agar yeh output aa gaya — 🎉 **aapka setup complete hai. Aage badho.**

---

## Agar error aaye

| Error | Matlab | Fix |
|-------|--------|-----|
| `g++: command not found` | compiler installed nahi hai / PATH mein nahi hai | upar wala install step dobara karo |
| `permission denied` | file executable nahi hai | `chmod +x test` chalao |
| `No such file or directory` | galat folder mein ho | `pwd` aur `ls` se check karo |
| `'test' is not recognized` | Windows pe `./test` kaam nahi karta | `.\test.exe` try karo |
| Compile hua par output nahi | shayad `./` bhool gaye | `./test` likho, sirf `test` nahi |

---

## Ek chhota helper: Makefile

Har baar poora `g++ -std=c++20 -Wall ...` likhna boring hai. Repo ke root mein ek
`Makefile` diya hai. Use karo:

```bash
# root folder se
make FILE=02-CPP-FIRST-STEPS/examples/01_hello_world.cpp
```

Yeh compile bhi karega aur chala bhi dega.

(`make` kya hai — yeh folder 24 mein detail mein padhenge. Abhi bas ek shortcut samajh lo.)

---

## Optional: Git (baad ke liye)

Git aapke code ka "time machine" hai. Abhi zaruri nahi, lekin folder 24 tak aate aate
seekh lena.

```bash
git --version     # already installed hai?
```

Agar nahi hai to upar wale install commands mein `git` shaamil hai.

---

## Exercises

1. Terminal mein `pwd`, `ls`, `cd ~`, `mkdir`, `cd` — sab try karo. 5 baar.
2. Ek folder banao `cpp-practice/day1`, uske andar jao, `pwd` se confirm karo.
3. `test.cpp` mein message badal do ("Mera naam ___ hai"), dobara compile karo, chalao.
4. `g++ --version` chalao. Aapke paas kaunsa version hai? Note kar lo.
5. `-std=c++20` hata ke compile karo. Kya fark pada? (Abhi shayad kuch nahi — lekin aage padega.)

---

## Next

→ [`../01-PROGRAMMING-BASICS/00-README.md`](../01-PROGRAMMING-BASICS/00-README.md)

Ab asli course shuru hota hai.
