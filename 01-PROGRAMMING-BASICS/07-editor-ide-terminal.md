# 07 — Editor, IDE, aur Terminal

## Prerequisites
`04-source-code-and-files.md`

## Yeh topic abhi kyun
Aap code likhne wale ho. Kis cheez mein likhoge? Aur chalaoge kaise? Yeh teen tools
aapke roz ke saathi banenge.

---

## 1. Editor kya hai?

**Editor = text likhne ka program.**

Bas. Notepad bhi editor hai. Lekin code likhne wale editors mein extra features hote hain:

| Feature | Kya karta hai |
|---|---|
| **Syntax highlighting** | keywords ko rang deta hai — padhna aasan |
| **Auto-indent** | code ko apne aap sahi jagah pe daalta hai |
| **Bracket matching** | `{` ke saath uska `}` highlight karta hai |
| **Line numbers** | error messages line number dete hain — bahut zaroori |
| **Find & replace** | poori file mein search |
| **Autocomplete** | half-typed naam complete karta hai |

Popular code editors: **VS Code**, Sublime Text, Vim, Neovim, Emacs, Notepad++

---

## 2. IDE kya hai?

**IDE = Integrated Development Environment**

IDE = Editor + Compiler integration + Debugger + Build system + Project management,
sab ek jagah.

```
   EDITOR (bas text likho)
      +
   COMPILER shortcut (ek button se compile)
      +
   DEBUGGER (code ko line by line chalao, values dekho)
      +
   PROJECT MANAGER (bahut saari files organize karo)
      +
   AUTOCOMPLETE / IntelliSense (smart suggestions)
      =
   IDE
```

Popular C++ IDEs: **CLion** (JetBrains), Visual Studio (Windows), Qt Creator, Code::Blocks

### VS Code — beech ka raasta

VS Code technically ek **editor** hai, lekin extensions ke saath **IDE jaisa** ban jaata hai.
Isliye hum yahi recommend karte hain — halka bhi hai aur powerful bhi.

---

## 3. Terminal kya hai?

**Terminal = computer ko likhkar commands dene ki jagah.**

```
   GUI (Graphical User Interface)       TERMINAL (Command Line)
   ------------------------------       -----------------------
   folder pe double click                cd folder
   right click -> New Folder             mkdir naya
   file drag karke delete                rm file.txt
   Ctrl+C, Ctrl+V                        cp source dest
```

### Terminal kyun seekhein? (jab GUI easy hai)

1. **Compiler terminal se hi chalta hai.** `g++ file.cpp -o file` — koi button nahi hai.
2. **Servers mein GUI hoti hi nahi.** HFT servers pe sirf terminal hoti hai. Bas.
3. **Automation** — ek command se 100 files process kar sakte ho
4. **Speed** — expert log terminal mein GUI se 5x tez kaam karte hain
5. **Remote access** — SSH se doosre computer pe kaam karna

> **HFT reality check:** Aap ek trading server pe log in karoge SSH se. Koi mouse nahi
> hoga, koi window nahi hogi. Sirf ek black screen. Agar aapko terminal nahi aati,
> aap kaam nahi kar sakte. Isliye abhi se aadat daalo.

---

## Terminal ka anatomy

```
   user@laptop:~/cpp-practice$ g++ hello.cpp -o hello
   ^^^^ ^^^^^^ ^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^
   |    |      |              |
   |    |      |              +-- aapki command
   |    |      +-- current directory (kahan ho)
   |    +-- computer ka naam
   +-- aapka username

   Yeh poora hissa "prompt" kehlata hai. Yeh aapse input maang raha hai.
```

`$` ka matlab: "normal user". Agar `#` dikhe to aap root (admin) ho — **dhyaan se**.

---

## Terminal — jo abhi jaanna zaroori hai

### Navigation
```bash
pwd                 # Print Working Directory - "main kahan hoon?"
ls                  # list - "yahan kya hai?"
ls -la              # sab kuch, details ke saath (hidden files bhi)
cd foldername       # andar jao
cd ..               # ek level upar
cd ~                # home pe jao
cd -                # pichli jagah wapas
```

### Files banana/dekhna
```bash
mkdir newfolder     # folder banao
mkdir -p a/b/c      # nested folders ek saath
touch file.cpp      # khali file banao
cat file.cpp        # poora content dikhao
head -20 file.cpp   # pehli 20 lines
tail -20 file.cpp   # aakhri 20 lines
less file.cpp       # scroll karke padho (q dabao nikalne ke liye)
```

### Files manage karna
```bash
cp a.cpp b.cpp      # copy
mv a.cpp b.cpp      # rename ya move
rm file.cpp         # DELETE (⚠️ trash mein nahi jaata, hamesha ke liye jaata hai)
rm -r folder        # folder delete
```

### C++ ke liye
```bash
g++ -std=c++20 -Wall -Wextra -g hello.cpp -o hello   # compile
./hello                                              # chalao
echo $?                                              # pichle program ka return code
```

### Life-saving shortcuts
| Key | Kya karta hai |
|---|---|
| `Tab` | naam auto-complete karo (**sabse useful**) |
| `↑` / `↓` | pichli commands |
| `Ctrl+C` | chalta hua program rok do |
| `Ctrl+L` ya `clear` | screen saaf |
| `Ctrl+A` / `Ctrl+E` | line ke start / end pe jao |
| `Ctrl+R` | purani command search karo |

---

## `./hello` mein `./` kyun?

Yeh beginner ka classic sawal hai.

Jab aap `ls` likhte ho, terminal ek list mein dhoondhta hai (`PATH` variable) —
`/usr/bin`, `/bin`, etc. Wahan `ls` mil jaata hai.

Aapka `hello` **us list mein nahi hai**. Woh current folder mein hai.

`./` ka matlab: **"current folder"**.
`./hello` = "iss folder wala hello chalao"

Security ke liye current folder PATH mein nahi hota — warna koi aapke folder mein
`ls` naam ka virus rakh de to aap galti se chala doge.

---

## VS Code setup — quick

1. VS Code kholo
2. `File → Open Folder` → apna `cpp-practice` folder chuno
3. `Ctrl+`` ` `` (backtick) dabao → **terminal VS Code ke andar hi khul jayega** 🎉
4. Ab aap ek hi window mein likh bhi sakte ho aur compile bhi kar sakte ho

### Useful VS Code shortcuts
| Shortcut | Kya |
|---|---|
| `Ctrl+S` | save |
| `Ctrl+`` ` `` | terminal toggle |
| `Ctrl+/` | line comment/uncomment |
| `Alt+↑/↓` | line upar/neeche move karo |
| `Shift+Alt+↓` | line duplicate |
| `Ctrl+D` | agla same word select karo |
| `F12` | definition pe jao |
| `Ctrl+Shift+P` | command palette (sab kuch yahan se) |

---

## Editor / IDE choose kaise karein

| Situation | Recommendation |
|---|---|
| Bilkul beginner | **VS Code** — aasan, free, popular |
| Bade projects, CMake | CLion (paid, students free) ya VS Code + CMake Tools |
| Server pe SSH se kaam | **Vim/Neovim** seekhna padega |
| Windows-only development | Visual Studio |

**Meri salah:** Abhi VS Code use karo. Folder 24-29 tak aate-aate basic Vim seekh lena
(`vimtutor` command chalao — 30 minute ka built-in tutorial hai). HFT servers pe kaam
aayega.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "IDE zaroori hai C++ ke liye" | Nahi. Notepad + terminal se bhi ho jaata hai |
| "Terminal hackers ke liye hai" | Terminal har developer ka roz ka tool hai |
| "`rm` se file trash mein jaati hai" | ❌ **NAHI.** Hamesha ke liye delete hoti hai. Careful! |
| "VS Code ek IDE hai" | Technically editor hai, extensions se IDE-jaisa banta hai |
| "Bade editor = better code" | Tool matter nahi karta. Understanding karti hai |

---

## Exercises

1. Terminal se yeh sab karo (GUI use kiye bina):
   ```bash
   cd ~
   mkdir -p cpp-practice/exercises/day1
   cd cpp-practice/exercises/day1
   touch a.cpp b.cpp c.cpp
   ls -la
   pwd
   cd ../..
   pwd
   ```

2. VS Code mein `cpp-practice` folder kholo. `Ctrl+`` ` `` se terminal kholo.
   Wahin se `ls` chalao. Kya dikha?

3. `Tab` completion practice: `cd cpp-p` likh ke `Tab` dabao. Kya hua?

4. `↑` arrow 5 baar dabao. Kya dikha?

5. **Danger drill:** Ek dummy file banao aur delete karo:
   ```bash
   touch delete_me.txt
   ls
   rm delete_me.txt
   ls
   ```
   Ab samjho: yeh file kahin nahi gayi. Gayab ho gayi. Isliye `rm` se hamesha dhyaan se.

6. `vimtutor` chalao (agar Linux/Mac/WSL pe ho). Pehle 2 lessons karo. 15 minute.
   Abhi zaroori nahi, par aage kaam aayega.

---

## Next
→ [`08-what-happens-when-you-press-run.md`](08-what-happens-when-you-press-run.md)
