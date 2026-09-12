# 11 — Tries (prefix trees)

## Prerequisites
- [`09-trees-and-bst.md`](09-trees-and-bst.md), [`08-hash-tables.md`](08-hash-tables.md)
- Folder 10 (strings), folder 19 file 22 (`<bit>`)

## Yeh topic abhi kyun
Trie = ek tree jahan **path hi key hoti hai**. String set / map ke liye,
`O(L)` lookup (L = string length) — hash ke `O(L)` jitna hi, par **prefix
queries** free milti hain: autocomplete, "sab keys with prefix X", longest
matching prefix. HFT mein symbol lookup aur routing tables isse relate karte.

---

## The idea

Store a set of strings so that **common prefixes are shared**. Each edge is
labelled with a character; each node represents the prefix spelled by the path
from the root. A flag marks nodes that are the end of an inserted word.

```
insert: "cat", "car", "card", "dog"

            (root)
           /      \
         c          d
         |          |
         a          o
        / \         |
       t*  r        g*
           |
           d*
   (* = end of a word)
```

```cpp
struct TrieNode {
    std::array<TrieNode*, 26> next{};   // a-z; or a hash map for a large alphabet
    bool isWord = false;
};

void insert(TrieNode* root, std::string_view w) {
    TrieNode* n = root;
    for (char c : w) {
        int k = c - 'a';
        if (!n->next[k]) n->next[k] = new TrieNode{};
        n = n->next[k];
    }
    n->isWord = true;
}

bool contains(const TrieNode* root, std::string_view w) {
    const TrieNode* n = root;
    for (char c : w) {
        n = n->next[c - 'a'];
        if (!n) return false;
    }
    return n->isWord;
}

bool startsWith(const TrieNode* root, std::string_view prefix) {
    const TrieNode* n = root;
    for (char c : prefix) { n = n->next[c - 'a']; if (!n) return false; }
    return true;   // the prefix path exists
}
```

- **insert / contains / startsWith**: `O(L)` — one node hop per character, **no
  hashing, no collisions**.
- **prefix enumeration** ("all words starting with `car`"): walk to the prefix
  node, then DFS the subtree collecting `isWord` nodes.
- **space**: up to `O(total characters × alphabet)` in the naive array form — a
  26-pointer array per node is 208 bytes even for one child.

---

## Variants

- **Map per node** (`std::unordered_map<char, Node*>` or a small sorted vector)
  instead of a fixed array — for large/unknown alphabets (Unicode, bytes), or
  sparse tries. Saves memory, costs a small per-hop lookup.
- **Compressed trie / radix tree (PATRICIA)** — collapse chains of single-child
  nodes into one edge labelled with a substring. `"card"` alone → root `-card→`
  leaf, not four nodes. Far less memory and fewer hops for sparse key sets. Used
  in IP routing tables (`longest-prefix match`), `git`'s object store, some
  database indexes.
- **Ternary search trie** — each node has `<`, `=`, `>` children (like a BST on
  the current character). Between a trie and a BST; memory-efficient for large
  alphabets.
- **Bitwise trie** — branch on bits of an integer key instead of characters. A
  16-bit-fan-out bitwise trie (a "multibit trie") over IP prefixes gives
  hardware-router-style longest-prefix match in a few memory accesses.
- **Suffix trie / suffix automaton / suffix array** — index *all substrings* of
  one text for `O(pattern)` substring search (file 16 covers the array form).

---

## What a trie gives you that a hash set doesn't

| Query | Hash set | Trie |
|---|---|---|
| exact membership | `O(L)` | `O(L)` |
| "does any key have prefix P?" | `O(n·L)` scan | **`O(P)`** |
| "all keys with prefix P" | `O(n·L)` scan | `O(P + output)` |
| "longest key that is a prefix of S" | not directly | **`O(S)`** — walk S, remember last `isWord` |
| keys in sorted order | need to sort | DFS in child order = sorted |
| autocomplete-as-you-type | rebuild each keystroke | descend one node per keystroke |

The hash set wins on raw membership constant factor (one hash + one probe vs `L`
pointer hops) and memory. The trie wins whenever **prefixes** are the question.

---

## Andar kya hota hai

- A trie lookup is `L` **pointer chases** — `n = n->next[k]`. Nodes are
  separately allocated → a likely cache miss per character, same problem as a
  linked list (file 06). For short keys (symbols ≤ 8 chars) that's ≤ 8 misses;
  for long keys it adds up.
- The 26-pointer (or 256-pointer) array node is cache-unfriendly and wastes
  memory on sparse tries — most `next[]` slots are null. A compressed/radix trie
  or a small-map node fixes both.
- **Flattening**: store all nodes in one `std::vector<TrieNode>` and use
  `uint32` indices instead of pointers (same trick as the arena linked list,
  file 06). Contiguous storage → the child of node `i` is often prefetchable, and
  building the trie is one allocation, not `N`.
- A **double-array trie** (Aoe's structure) packs the transition table into two
  parallel `int` arrays so a transition is two array reads with no pointer chase
  — this is what fast production tries (MeCab, Darts) use.

> **HFT relevance:** tries appear where **prefix or longest-match** semantics are
> needed: **symbol lookup / normalization** (ticker → internal id, with prefix
> completion for UIs), **order routing / venue selection** by instrument prefix,
> and **IP longest-prefix match** in network-layer code (multibit / compressed
> bitwise tries, or offloaded to the NIC/switch). On the hot path they're the
> **flattened / double-array** form — contiguous index-linked nodes, no
> per-character `malloc` — or replaced entirely by a **perfect hash** built at
> startup when the symbol set is fixed and known (no prefix queries needed at
> runtime). A pointer-per-node `std::unordered_map<char,Node*>` trie is a
> prototype, not production.

---

## Hands-on

```bash
# no dedicated example -- build one:
```
```cpp
// trie.cpp
#include <array>
#include <cstdio>
#include <string_view>
#include <vector>

struct Trie {
    struct Node { std::array<int,26> next; bool word; };
    std::vector<Node> nodes{ {{}, false} };            // node 0 = root
    int newNode() { nodes.push_back({{}, false}); for (auto& x : nodes.back().next) x = 0; return (int)nodes.size()-1; }
    void insert(std::string_view w) {
        int cur = 0;
        for (char c : w) { int& nx = nodes[(size_t)cur].next[(size_t)(c-'a')];
                           if (!nx) nx = newNode(); cur = nx; }
        nodes[(size_t)cur].word = true;
    }
    bool contains(std::string_view w) const {
        int cur = 0;
        for (char c : w) { int nx = nodes[(size_t)cur].next[(size_t)(c-'a')];
                           if (!nx) return false; cur = nx; }
        return nodes[(size_t)cur].word;
    }
};
int main() {
    Trie t; for (auto s : {"cat","car","card","dog"}) t.insert(s);
    for (auto s : {"car","care","do","dog"}) std::printf("%-5s %d\n", s, t.contains(s));
}
```
```bash
g++ -std=c++20 -O2 -Wall trie.cpp -o trie && ./trie   # cat->prefix-flatten form (index links, one vector)
```

Add `startsWith`, prefix enumeration (DFS the subtree), and "longest key that is
a prefix of S".

---

## ⚠️ Traps

### Trap 1 — 26/256-pointer array node for a sparse trie
```cpp
std::array<Node*, 256> next{};   // ⚠️ 2 KB per node, mostly null. Use a small map or a radix trie for sparse keys
```

### Trap 2 — pointer-per-node with per-character `new`
```cpp
// N characters -> N allocations, N scattered nodes -> a cache miss per char. Flatten into one vector, index links
```

### Trap 3 — `startsWith` vs `contains`
```cpp
// "car" path may exist without car being an inserted word (it's a prefix of "card").
// contains checks isWord; startsWith only checks the path exists.
```

### Trap 4 — forgetting the terminal flag
```cpp
// Without isWord, {"card"} would make contains("car") true. Mark end-of-word nodes.
```

### Trap 5 — trie where a hash set would do
```cpp
// If you only ever do exact membership, a hash set is simpler, less memory, and fewer cache misses.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "A trie stores strings in the nodes" | The **path** is the key; nodes hold only child links + a terminal flag |
| "Trie lookup beats a hash set" | Same `O(L)`; hash wins on constant factor + memory; trie wins on prefix queries |
| "`startsWith` implies the word was inserted" | It only means the prefix path exists |
| "The array-node trie is memory-efficient" | 26–256 pointers per node, mostly null — use radix/compressed or map nodes |
| "Tries are cache-friendly" | Pointer-per-node → a cache miss per character; flatten to fix |

---

## Exercises

1. **Longest prefix:** given a trie of words and a string `S`, find the longest
   inserted word that is a prefix of `S`.

   <details><summary>Answer</summary>

   Walk `S` character by character from the root; each time you land on an
   `isWord` node, record the current length. Stop when there's no child. Return
   `S.substr(0, bestLen)`.
   </details>

2. **Autocomplete:** given a prefix `P`, list up to `k` words with that prefix in
   sorted order.

   <details><summary>Answer</summary>

   Descend to the node for `P` (`O(|P|)`). DFS its subtree visiting children in
   `a..z` order; append `P + path` at each `isWord` node; stop after `k`.
   `O(|P| + k·L)`.
   </details>

3. **Space:** 100,000 words, average length 8, alphabet 26, naive array nodes.
   Rough node count and memory? How does a radix trie help?

   <details><summary>Answer</summary>

   Up to ~800,000 nodes (fewer with shared prefixes), each ~26 pointers = 208 B →
   ~150 MB worst case. A radix trie collapses single-child chains: only branch
   points get nodes → often 5–20× fewer nodes and much less null-pointer waste.
   </details>

4. **Trie vs hash:** you need "is `sym` a known ticker?" and nothing else, symbol
   set fixed at startup. Which structure?

   <details><summary>Answer</summary>

   A hash set (or a **perfect hash** built once from the fixed set) — exact
   membership only, so the trie's prefix powers are unused, and the hash has a
   smaller constant factor and footprint.
   </details>

5. **Flatten:** convert a pointer trie to index-linked nodes in one
   `std::vector<Node>`. What improves?

   <details><summary>Answer</summary>

   One allocation instead of `N`; child nodes tend to be near each other in the
   vector → better prefetch and fewer cache misses per character; links are
   `uint32` (half the size of a 64-bit pointer) → smaller nodes, more per cache
   line.
   </details>

---

## Interview questions

1. Trie mein key kahan store hoti (path)? Lookup complexity?
2. Trie hash set se kab better (prefix queries), kab worse (exact only)?
3. `contains` vs `startsWith` — terminal flag kyun zaroori?
4. Naive array-node trie ka memory problem — radix/compressed trie kaise fix karta?
5. Trie cache-friendly kyun nahi, flatten se kya milta?
6. Longest-prefix match (IP routing) — kaunsa trie variant?

---

## Next
→ [`12-graphs.md`](12-graphs.md)
