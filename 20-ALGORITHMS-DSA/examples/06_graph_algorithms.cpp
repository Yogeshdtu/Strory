// 06_graph_algorithms.cpp
// ============================================================
// Graph algorithms on a FLAT adjacency list (vector<vector<Edge>>):
// BFS (shortest path in edges), DFS (recursive + iterative),
// Dijkstra (priority_queue), and topological sort (Kahn).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_graph_algorithms.cpp -o g && ./g
// ============================================================

#include <cstdint>
#include <cstdio>
#include <limits>
#include <queue>
#include <vector>

struct Edge { int to; int w; };
using Graph = std::vector<std::vector<Edge>>;

// ---- BFS: fewest EDGES from src (unweighted) ----
static std::vector<int> bfs(const Graph& g, int src) {
    std::vector<int> dist(g.size(), -1);
    std::queue<int> q;
    dist[static_cast<std::size_t>(src)] = 0;
    q.push(src);
    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (const Edge& e : g[static_cast<std::size_t>(u)])
            if (dist[static_cast<std::size_t>(e.to)] < 0) {
                dist[static_cast<std::size_t>(e.to)] = dist[static_cast<std::size_t>(u)] + 1;
                q.push(e.to);
            }
    }
    return dist;
}

// ---- DFS recursive: preorder visit list ----
static void dfsRec(const Graph& g, int u, std::vector<char>& seen, std::vector<int>& order) {
    seen[static_cast<std::size_t>(u)] = 1;
    order.push_back(u);
    for (const Edge& e : g[static_cast<std::size_t>(u)])
        if (!seen[static_cast<std::size_t>(e.to)]) dfsRec(g, e.to, seen, order);
}

// ---- DFS iterative (explicit stack) ----
static std::vector<int> dfsIter(const Graph& g, int src) {
    std::vector<char> seen(g.size(), 0);
    std::vector<int>  order;
    std::vector<int>  st{src};
    while (!st.empty()) {
        int u = st.back(); st.pop_back();
        if (seen[static_cast<std::size_t>(u)]) continue;
        seen[static_cast<std::size_t>(u)] = 1;
        order.push_back(u);
        // push neighbours reversed so we visit them in listed order
        const auto& adj = g[static_cast<std::size_t>(u)];
        for (auto it = adj.rbegin(); it != adj.rend(); ++it)
            if (!seen[static_cast<std::size_t>(it->to)]) st.push_back(it->to);
    }
    return order;
}

// ---- Dijkstra: shortest WEIGHTED path, non-negative weights ----
static std::vector<long long> dijkstra(const Graph& g, int src) {
    const long long INF = std::numeric_limits<long long>::max();
    std::vector<long long> dist(g.size(), INF);
    using PQItem = std::pair<long long, int>;                 // (distance, node)
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<>> pq;
    dist[static_cast<std::size_t>(src)] = 0;
    pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[static_cast<std::size_t>(u)]) continue;  // stale entry (lazy decrease-key)
        for (const Edge& e : g[static_cast<std::size_t>(u)]) {
            long long nd = d + e.w;
            if (nd < dist[static_cast<std::size_t>(e.to)]) {
                dist[static_cast<std::size_t>(e.to)] = nd;
                pq.push({nd, e.to});
            }
        }
    }
    return dist;
}

// ---- Topological sort (Kahn's algorithm) on a DAG ----
static std::vector<int> topoSort(const Graph& g) {
    std::vector<int> indeg(g.size(), 0);
    for (const auto& adj : g)
        for (const Edge& e : adj) ++indeg[static_cast<std::size_t>(e.to)];
    std::queue<int> q;
    for (std::size_t i = 0; i < g.size(); ++i)
        if (indeg[i] == 0) q.push(static_cast<int>(i));
    std::vector<int> order;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        order.push_back(u);
        for (const Edge& e : g[static_cast<std::size_t>(u)])
            if (--indeg[static_cast<std::size_t>(e.to)] == 0) q.push(e.to);
    }
    return order;                                              // size < g.size() -> a cycle exists
}

static void printVec(const char* label, const std::vector<int>& v) {
    std::printf("  %-16s", label);
    for (int x : v) std::printf("%d ", x);
    std::printf("\n");
}

int main() {
    // Weighted directed graph, 7 nodes (0..6):
    //   0->1(2) 0->2(5) 1->2(1) 1->3(4) 2->3(2) 2->4(7) 3->5(3) 4->5(1) 5->6(2)
    Graph g(7);
    auto add = [&](int u, int v, int w){ g[static_cast<std::size_t>(u)].push_back({v, w}); };
    add(0,1,2); add(0,2,5); add(1,2,1); add(1,3,4); add(2,3,2);
    add(2,4,7); add(3,5,3); add(4,5,1); add(5,6,2);

    std::printf("=== BFS from 0 (edge count) ===\n");
    printVec("dist:", bfs(g, 0));

    std::printf("\n=== DFS from 0 ===\n");
    {
        std::vector<char> seen(g.size(), 0);
        std::vector<int>  order;
        dfsRec(g, 0, seen, order);
        printVec("recursive:", order);
        printVec("iterative:", dfsIter(g, 0));
    }

    std::printf("\n=== Dijkstra from 0 (weighted) ===\n");
    {
        auto d = dijkstra(g, 0);
        std::printf("  node : dist\n");
        for (std::size_t i = 0; i < d.size(); ++i)
            std::printf("   %zu   :  %lld\n", i, d[i]);
        std::printf("  (0->1->2->3->5->6 = 2+1+2+3+2 = 10)\n");
    }

    std::printf("\n=== Topological sort (this DAG) ===\n");
    {
        auto order = topoSort(g);
        printVec("order:", order);
        std::printf("  valid full order? %s\n", order.size() == g.size() ? "yes (DAG)" : "no (cycle)");
    }

    std::printf(
        "\n"
        "  Representation: vector<vector<Edge>> -- adjacency lists, contiguous per node.\n"
        "  BFS/topo use a queue; DFS a stack (or recursion); Dijkstra a min-heap with\n"
        "  lazy stale-entry skipping instead of a real decrease-key.\n"
        "  Complexities: BFS/DFS O(V+E), Dijkstra O((V+E) log V), Kahn O(V+E).\n");
    return 0;
}
