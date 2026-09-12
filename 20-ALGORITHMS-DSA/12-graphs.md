# 12 — Graphs: representation, BFS, DFS, Dijkstra, topo sort

## Prerequisites
- [`07-stacks-and-queues.md`](07-stacks-and-queues.md), [`10-heaps.md`](10-heaps.md)
- Folder 19 file 07 (`queue`, `priority_queue`), folder 19 file 02 (`vector`)

## Yeh topic abhi kyun
Graph = nodes + edges. Dependencies, networks, routes, state machines — sab
graphs. Char core algorithms (BFS, DFS, Dijkstra, topological sort) 80% graph
problems cover karte, aur `examples/06_graph_algorithms.cpp` mein saare ek flat
adjacency list pe implement hain.

---

## Representation

| | Adjacency **list** | Adjacency **matrix** |
|---|---|---|
| storage | `O(V + E)` | `O(V²)` |
| "is there an edge u→v?" | `O(deg u)` | `O(1)` |
| iterate a node's neighbours | `O(deg u)` | `O(V)` |
| best for | sparse graphs (`E ≪ V²`) — most real graphs | dense graphs, or when you need `O(1)` edge tests |

```cpp
// adjacency list -- the default
struct Edge { int to; int w; };
using Graph = std::vector<std::vector<Edge>>;      // g[u] = u's out-edges
```

`examples/06` uses this. For an **unweighted** graph drop `w` (or use
`std::vector<std::vector<int>>`). For a **compressed** form (best cache behaviour,
static graphs): CSR / "flat adjacency" — one `edges` array + a `start[]` index
array, so node `u`'s neighbours are `edges[start[u] .. start[u+1])`, fully
contiguous.

**Directed vs undirected**: undirected = add both `u→v` and `v→u`. **Weighted**:
each edge carries a cost.

---

## BFS — shortest path in **edge count** (unweighted)

Explore level by level with a **queue**. First time you reach a node is via a
shortest path.

```cpp
std::vector<int> bfs(const Graph& g, int src) {
    std::vector<int> dist(g.size(), -1);
    std::queue<int> q;
    dist[src] = 0; q.push(src);
    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (auto [v, w] : g[u])
            if (dist[v] < 0) { dist[v] = dist[u] + 1; q.push(v); }
    }
    return dist;
}
```

`O(V + E)`. Uses: shortest hops, connected components, bipartite check
(2-colouring), "nearest X", web crawl by depth. Multi-source BFS: push all
sources with distance 0.

---

## DFS — explore deep first (stack or recursion)

```cpp
void dfs(const Graph& g, int u, std::vector<char>& seen) {
    seen[u] = 1;
    // pre-order work here
    for (auto [v, w] : g[u]) if (!seen[v]) dfs(g, v, seen);
    // post-order work here
}
```

`O(V + E)`. Recursion depth up to `O(V)` → **stack overflow on large graphs**;
`examples/06` also shows the **iterative** version with an explicit
`std::vector<int>` stack.

Uses: cycle detection, topological sort (post-order), connected /
strongly-connected components (Tarjan / Kosaraju), bridges & articulation points,
maze/backtracking, tree/graph serialization. The **pre-order vs post-order**
distinction is where DFS earns its keep (subtree done → aggregate up).

---

## Dijkstra — shortest **weighted** path, non-negative weights

Greedy: repeatedly settle the unsettled node with the smallest tentative
distance, relax its edges. A **min-heap** (`std::priority_queue`) gives the
smallest quickly.

```cpp
std::vector<long long> dijkstra(const Graph& g, int src) {
    const long long INF = LLONG_MAX;
    std::vector<long long> dist(g.size(), INF);
    using Item = std::pair<long long,int>;                       // (dist, node)
    std::priority_queue<Item, std::vector<Item>, std::greater<>> pq;
    dist[src] = 0; pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;                               // stale entry -> skip
        for (auto [v, w] : g[u])
            if (d + w < dist[v]) { dist[v] = d + w; pq.push({dist[v], v}); }
    }
    return dist;
}
```

`O((V + E) log V)`. The `if (d > dist[u]) continue;` is **lazy decrease-key** —
`std::priority_queue` can't update an entry, so we push a new (smaller) one and
ignore the outdated copies when they surface (`examples/06` measured this on a
7-node graph: node 6 at distance 10 via `0→1→2→3→5→6`).

- **Negative weights** → Dijkstra is **wrong** (a settled node might be improvable
  later). Use **Bellman-Ford** (`O(V·E)`, detects negative cycles) or **SPFA**.
- **All-pairs** → **Floyd-Warshall** (`O(V³)`, tiny constant, dense-friendly).
- **Unweighted** → just BFS (`O(V+E)`, no heap).
- **0/1 weights** → 0-1 BFS with a deque (`O(V+E)`).
- **A\*** — Dijkstra + an admissible heuristic to steer toward the goal (routing,
  games).

---

## Topological sort — linear order of a DAG

An ordering where every edge `u→v` has `u` before `v`. Only exists if the graph
is a **DAG** (no cycles).

### Kahn's algorithm (BFS-style, `examples/06`)
```cpp
// compute in-degrees; queue all zero-in-degree nodes;
// pop u, append to order, decrement each neighbour's in-degree, enqueue new zeros.
// order.size() < V  <=>  a cycle exists.
```

### DFS-based
Do a DFS; **push each node onto a stack when its post-order (all descendants
done) completes**; the reversed stack is a topological order.

Uses: build systems / task scheduling (compile order), course prerequisites,
spreadsheet recalculation, dependency resolution, instruction scheduling.

---

## Andar kya hota hai

- **Adjacency list traversal** touches `g[u]` (a contiguous `std::vector<Edge>`)
  then jumps to `g[v]` for each neighbour — the outer jumps are unpredictable
  (graph-dependent), the inner neighbour scan is contiguous. CSR / flat adjacency
  makes the *whole* edge array contiguous → BFS/DFS become near-linear memory
  scans, 2–5× faster on large graphs than `vector<vector<>>`.
- BFS's `queue` and Kahn's `queue`: `std::queue` (deque-backed) allocates chunks;
  a preallocated ring buffer or a `std::vector` used as a two-pointer queue
  avoids that.
- Dijkstra's heap: `std::priority_queue` over a `std::vector` is contiguous and
  fast; the lazy-deletion `continue` means the heap can hold up to `O(E)` stale
  entries → memory `O(E)`, but each is popped in `O(log E)`. A **4-ary heap** or
  an **indexed heap** (real decrease-key) trims this for huge graphs.
- The `seen`/`dist` arrays are `O(V)` contiguous — random-access by node id, one
  cache line per few nodes. Fine unless `V` is huge and access is scattered.

> **HFT relevance:** graph algorithms are **offline / config-time** in trading
> systems, not tick-path: **topological sort** to order strategy/component
> initialization and to schedule a calculation DAG (a signal that depends on
> other signals — recompute in topo order); **BFS/DFS** over an instrument or
> venue graph at startup; **Dijkstra / shortest-path** for smart-order-routing
> cost models and for network path planning, computed periodically, not per
> order. When any of this is hot (rare), it's on a **CSR flat adjacency** with
> preallocated work arrays and no allocation in the loop. The p99-latency
> discipline (bounded work, no allocation, contiguous memory) still applies.

---

## Hands-on

```bash
./build.ps1 20-ALGORITHMS-DSA/examples/06_graph_algorithms.cpp
```

Implement BFS, recursive + iterative DFS, Dijkstra, and Kahn's topo sort on the
`vector<vector<Edge>>` graph. Then add: connected-components count (BFS/DFS from
each unvisited node), cycle detection in a directed graph (DFS with a
`in-stack` colour), and bipartite check (2-colour BFS).

---

## ⚠️ Traps

### Trap 1 — Dijkstra with negative edge weights
```cpp
// A settled node can still be improved via a negative edge later -> wrong distances. Use Bellman-Ford.
```

### Trap 2 — recursive DFS on a big graph
```cpp
dfs(g, src, seen);   // ⚠️ depth up to V -> stack overflow at ~100k+ nodes. Iterative with an explicit stack
```

### Trap 3 — marking `seen` when popping instead of when pushing (BFS)
```cpp
// BFS: mark seen (set dist) when you ENQUEUE, not when you dequeue -- else a node gets queued many times.
```

### Trap 4 — forgetting the stale-entry skip in heap Dijkstra
```cpp
auto [d, u] = pq.top(); pq.pop();
// process u ...   // ⚠️ without `if (d > dist[u]) continue;` you reprocess u for every outdated push -> slow / wrong
```

### Trap 5 — topo sort on a graph with a cycle
```cpp
// Kahn's `order` will have fewer than V nodes -> that's your cycle detector. Check order.size() == V.
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "BFS finds the shortest weighted path" | BFS = fewest **edges**; weighted needs Dijkstra/Bellman-Ford |
| "Dijkstra works with negative weights" | No — use Bellman-Ford (also detects negative cycles) |
| "DFS and BFS have different complexity" | Both `O(V + E)` — they differ in *order*, not cost |
| "`priority_queue` supports decrease-key" | It doesn't — Dijkstra uses lazy deletion (push + skip stale) |
| "Topological sort always exists" | Only for a DAG; a cycle → no valid order (Kahn leaves nodes unprocessed) |

---

## Exercises

1. **BFS vs Dijkstra:** all edge weights are 1. Which do you use and why?

   <details><summary>Answer</summary>

   BFS — with unit weights, fewest edges = shortest distance, and BFS is
   `O(V+E)` with no heap. Dijkstra would give the same answer but pays a
   `log V` factor for nothing.
   </details>

2. **Cycle in a DAG-to-be:** you run Kahn's on a task graph and `order` has 7 of
   10 nodes. What does that mean and which nodes are the problem?

   <details><summary>Answer</summary>

   There's a cycle — the 3 unprocessed nodes (and any reachable only through
   them) never hit in-degree 0. Those 3 form (or feed into) a dependency cycle
   that must be broken.
   </details>

3. **Dijkstra correctness:** why does settling the smallest-tentative-distance
   node guarantee its distance is final (with non-negative weights)?

   <details><summary>Answer</summary>

   Any other path to it must go through some currently-unsettled node `x` with
   `dist[x] ≥ dist[u]`, and all remaining edges are `≥ 0`, so that path is `≥
   dist[u]`. Hence `dist[u]` can't be improved → it's final. Breaks with negative
   edges (a later negative edge could undercut it).
   </details>

4. **Iterative DFS order:** why push neighbours in reverse to match the recursive
   visit order?

   <details><summary>Answer</summary>

   A stack is LIFO; pushing `v1, v2, v3` pops `v3` first. Push `v3, v2, v1` so
   `v1` pops first, matching recursion's "visit the first neighbour first".
   </details>

5. **CSR:** convert `vector<vector<Edge>>` to CSR (`start[]`, `edges[]`). What
   improves for BFS on a huge graph?

   <details><summary>Answer</summary>

   `start[u]` = prefix sum of degrees; `edges[]` = all edges concatenated in node
   order. Node `u`'s neighbours are `edges[start[u]..start[u+1])` — one
   contiguous block, and the whole edge set is one allocation → BFS becomes a
   near-linear streaming scan, far fewer cache misses.
   </details>

---

## Interview questions

1. Adjacency list vs matrix — storage, edge-test cost, kab kaunsa?
2. BFS shortest path kyun deta (unweighted), Dijkstra kyun chahiye (weighted)?
3. Dijkstra negative weights pe kyun fail, alternative kya?
4. `std::priority_queue` mein decrease-key nahi — Dijkstra kaise handle karta?
5. Topological sort — Kahn vs DFS, cycle kaise detect hota?
6. Recursive DFS ka stack risk — iterative kaise, neighbours reverse kyun push?

---

## Next
→ [`13-greedy-algorithms.md`](13-greedy-algorithms.md)
