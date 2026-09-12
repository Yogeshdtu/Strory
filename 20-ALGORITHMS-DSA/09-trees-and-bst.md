# 09 — Trees and binary search trees

## Prerequisites
- [`06-linked-lists.md`](06-linked-lists.md), [`08-hash-tables.md`](08-hash-tables.md)
- Folder 19 file 05 (`std::map` = red-black tree), folder 07 (recursion)

## Yeh topic abhi kyun
Tree = recursive pointer structure. BST ordered data structure ka foundation hai
— `std::map`/`std::set` red-black trees hain. Traversals, BST operations, aur
**balancing kyun zaroori** — `examples/05_bst.cpp` mein 1023 ascending inserts se
height 1023 (ek linked list), middle-out se height 10.

---

## Vocabulary

```
            50            <- root
          /    \
        30      70        <- internal nodes
       /  \    /  \
      20  40  60  80      <- leaves
```

- **Height** — longest root-to-leaf path (edges). A tree of `n` nodes has height
  `≥ ⌊log₂ n⌋` (perfectly balanced) and `≤ n-1` (degenerate = linked list).
- **Depth** — edges from the root to a node.
- **Balanced** — height `O(log n)`. Requires self-balancing on insert/erase.
- **Binary tree** — ≤ 2 children. **Complete** — all levels full except possibly
  the last, filled left-to-right (heaps, file 10). **Perfect** — all leaves at
  the same depth.

```cpp
struct Node { int key; Node* left = nullptr; Node* right = nullptr; };
```

---

## The four traversals

```cpp
void preorder (Node* n) { if(!n) return; visit(n); preorder(n->left);  preorder(n->right); }  // N L R
void inorder  (Node* n) { if(!n) return; inorder(n->left);  visit(n); inorder(n->right);  }   // L N R
void postorder(Node* n) { if(!n) return; postorder(n->left); postorder(n->right); visit(n); } // L R N
// level-order (BFS): a queue
void levelorder(Node* root) {
    std::queue<Node*> q; if (root) q.push(root);
    while (!q.empty()) { Node* n = q.front(); q.pop(); visit(n);
        if (n->left) q.push(n->left); if (n->right) q.push(n->right); }
}
```

- **Preorder** — copy/serialize a tree (root before children).
- **Inorder** — **on a BST, yields keys in sorted order** (`examples/05` shows
  `20 30 35 40 45 50 60 70 80`).
- **Postorder** — delete/free a tree (children before parent), evaluate an
  expression tree, compute subtree aggregates.
- **Level-order** — shortest paths, "print by rows", tree width.

Each is `O(n)` time. Recursive traversals use `O(height)` stack — `O(log n)`
balanced, `O(n)` degenerate (stack-overflow risk). Iterative versions use an
explicit stack/queue.

**Morris traversal** does inorder in `O(1)` space by temporarily threading
leaf `right` pointers back to their inorder successor — niche but a real trick.

---

## Binary search tree

**BST property:** for every node, all keys in the left subtree are `< key`, all
in the right are `> key`.

```cpp
// find -- O(height)
bool find(Node* n, int key) {
    while (n) {
        if      (key == n->key) return true;
        else if (key <  n->key) n = n->left;
        else                    n = n->right;
    }
    return false;
}

// insert -- walk to a null child, attach
void insert(Node*& root, int key) {
    Node** cur = &root;
    while (*cur) cur = (key < (*cur)->key) ? &(*cur)->left
                     : (key > (*cur)->key) ? &(*cur)->right
                     : /* dup */ &(*cur = *cur, nullptr);   // stop on dup (simplified)
    *cur = new Node{key};
}
```

### Delete — three cases
1. **Leaf** — just remove it.
2. **One child** — splice: replace the node with its child.
3. **Two children** — replace the node's key with its **in-order successor**
   (leftmost node of the right subtree — the smallest key `> key`), then delete
   that successor (which has ≤ 1 child). Symmetric with the predecessor.

`examples/05_bst.cpp` implements all three via a `Node**` "pointer to the link",
which removes the special-case for deleting the root.

Complexity: everything is `O(height)`. **Balanced → `O(log n)`. Unbalanced →
`O(n)`.**

---

## Why balancing matters

`examples/05_bst.cpp`, section 4:

| insert order | height (n=1023) |
|---|---|
| ascending (`1, 2, 3, …`) | **1023** — a right-spine linked list |
| middle-out (`512, 256, 768, …`) | **10** — `log₂(1024)` |

A plain BST has **no self-balancing** — sorted or nearly-sorted input degrades it
to `O(n)` per operation. Real ordered containers fix this:

- **Red-black tree** (`std::map`, `std::set`) — nodes coloured red/black,
  invariants keep height `≤ 2·log₂(n+1)`. `O(1)` amortized rotations per
  insert/erase. Slightly taller than AVL but fewer rotations → better for
  write-heavy.
- **AVL tree** — stricter balance (subtree heights differ by ≤ 1), shorter →
  faster lookups, more rotations on writes.
- **B-tree / B+-tree** — high fan-out (many keys per node), shallow → few cache
  misses / disk seeks. The basis of database indexes and filesystem structures.
- **Treap, skip list** — randomized balance, simpler to implement.

---

## Andar kya hota hai

- BST `find` is a **pointer chase per level**. Balanced → `log n` levels → `log
  n` cache misses (nodes scattered on the heap). This is why `std::map` lookup is
  ~3× a sorted-`std::vector` binary search that does the same `log n` comparisons
  in ~2 cache lines (folder 19 files 05, 25).
- Rotations (red-black/AVL) are `O(1)` — 2–3 pointer reassignments — but the
  re-balance walk back toward the root is `O(log n)` pointer chases.
- `std::map` node ≈ `key + value + 3 pointers (left, right, parent) + colour` +
  allocator header ≈ 40+ bytes for `map<int,int>`. Every insert is a `new`.
- Recursion depth: a recursive traversal of a degenerate tree (`examples/05`'s
  ascending-insert case, height 1023) uses 1023 stack frames — fine here, but a
  height-100k tree overflows the stack. Iterative + explicit stack, or keep the
  tree balanced.

> **HFT relevance:** balanced BSTs (`std::map`/`std::set`) are **control-plane**
> structures — symbol metadata, session tables, config keyed by name — where
> ordered iteration and range queries are convenient and lookups are rare. On the
> hot path they lose to flatter layouts: a **sorted `std::vector` + binary
> search** (same `O(log n)`, ~2 cache misses, `front()`/`back()` = best bid/ask
> in `O(1)`), or a **flat array indexed by price ticks** (`O(1)`, zero search)
> for an order book. B-tree-style **high-fan-out** nodes (many keys per cache
> line) are the trick when you *do* need a tree with good locality. Traversal
> algorithms (inorder = sorted, postorder = subtree aggregates) are still the
> right mental model; it's the pointer-per-node layout that gets replaced.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/05_bst.cpp
```

Implement insert / find / the 3-case delete / all 4 traversals. Then: build a
BST from `1..1023` ascending and print its height (expect 1023); build one with
middle-out insertion (expect 10). Write "is this a valid BST?" (inorder must be
strictly increasing).

---

## ⚠️ Traps

### Trap 1 — assuming a plain BST stays balanced
```cpp
for (int i = 1; i <= n; ++i) bst.insert(i);   // ⚠️ ascending -> height n -> every op O(n). Use std::map (red-black)
```

### Trap 2 — deleting a two-child node wrong
```cpp
// You can't just remove it. Replace its key with the in-order successor's key,
// then delete the successor (which has <= 1 child).
```

### Trap 3 — recursive traversal on a deep/degenerate tree
```cpp
inorder(root);   // ⚠️ height ~ n -> stack overflow for large skewed trees. Iterative + explicit stack
```

### Trap 4 — "validate BST" checking only immediate children
```cpp
// return n->left->key < n->key && n->right->key > n->key;   // ⚠️ misses violations deeper down.
// Pass down (min, max) bounds, or check that an inorder walk is strictly increasing.
```

### Trap 5 — treating `std::map` as a hash table (perf)
```cpp
// map lookup is O(log n) with ~log n cache misses -- several x slower than unordered_map / sorted vector for pure lookup.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "A BST is `O(log n)`" | Only if balanced; a plain BST is `O(n)` on sorted input |
| "`std::map` is a hash map" | Red-black **tree** — sorted, `O(log n)`, pointer-chasing |
| "Inorder traversal works on any binary tree for sorting" | It yields sorted keys **only for a BST** |
| "Deleting a node = free it" | Two-child case needs successor replacement first |
| "Validate BST = check parent vs its two children" | Must enforce the bound transitively (min/max, or inorder-increasing) |

---

## Exercises

1. **Traversal from output:** preorder `[50,30,20,40,70,60,80]`, inorder
   `[20,30,40,50,60,70,80]`. Reconstruct the tree.

   <details><summary>Answer</summary>

   Preorder[0]=50 is the root. In inorder, 50 splits into left `[20,30,40]` and
   right `[60,70,80]`. Recurse: left root = next preorder = 30, etc. → the
   balanced tree from `examples/05`.
   </details>

2. **Delete cases:** in `examples/05`'s tree, delete 20 (leaf), 70 (two
   children), 30. What replaces 70?

   <details><summary>Answer</summary>

   20: removed directly. 70: two children → in-order successor is 80 (leftmost of
   the right subtree {80}); 80 takes 70's place. 30: after earlier deletes it has
   one child (40 with subtree) → spliced out. Final inorder: `35 40 45 50 60 80`.
   </details>

3. **Balance:** you must insert `1..1000` in ascending order into a BST and keep
   it `O(log n)` tall. Options?

   <details><summary>Answer</summary>

   Use a self-balancing tree (`std::map` / red-black, or AVL). Or, since the
   input is sorted, build a balanced BST directly by recursively taking the
   middle element as the root (`examples/05`'s middle-out `ins(lo, hi)`).
   </details>

4. **Validate BST:** write it with min/max bounds.

   <details><summary>Answer</summary>

   `bool ok(Node* n, long lo, long hi) { if (!n) return true; if (n->key <= lo ||
   n->key >= hi) return false; return ok(n->left, lo, n->key) && ok(n->right,
   n->key, hi); }` — call `ok(root, LONG_MIN, LONG_MAX)`.
   </details>

5. **Cache:** `std::map` and a sorted `std::vector` both do `log n` comparisons
   for a lookup. Why is the vector ~3× faster (folder 19 file 25)?

   <details><summary>Answer</summary>

   Binary search over a contiguous array touches elements sharing ~2 cache lines
   (especially the last steps); the tree's `log n` compared nodes are separate
   heap allocations → `log n` independent cache misses.
   </details>

---

## Interview questions

1. Tree height ka range (`log n` se `n-1`) — balanced kyun matter karta?
2. Char traversals — kaunsa kis kaam ke liye, inorder BST pe kya deta?
3. BST delete ke 3 cases — two-child case kaise?
4. Plain BST vs red-black tree — balancing ka fark?
5. `std::map` lookup sorted-`vector` se slow kyun (cache)?
6. Recursive traversal ka stack risk — kab, kaise avoid?

---

## Next
→ [`10-heaps.md`](10-heaps.md)
