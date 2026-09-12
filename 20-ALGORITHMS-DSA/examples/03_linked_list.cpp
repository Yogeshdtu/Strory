// 03_linked_list.cpp
// ============================================================
// Singly linked list -- built by hand, with the operations that
// show WHY a linked list exists (O(1) splice / front ops) and why
// it usually loses to std::vector (a cache miss per node).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 03_linked_list.cpp -o ll && ./ll
//   BENCH: g++ -std=c++20 -O2 03_linked_list.cpp -o ll && ./ll
// ============================================================

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

using Clock = std::chrono::steady_clock;

// ------------------------------------------------------------
//  A minimal owning singly linked list.
// ------------------------------------------------------------
class IntList {
    struct Node {
        int   value;
        Node* next;
    };
    Node*       head_ = nullptr;
    Node*       tail_ = nullptr;      // keep a tail so push_back is O(1)
    std::size_t size_ = 0;

public:
    IntList() = default;
    IntList(const IntList&)            = delete;   // Rule of Five: keep it simple, non-copyable
    IntList& operator=(const IntList&) = delete;
    ~IntList() { clear(); }

    void clear() {
        for (Node* p = head_; p;) { Node* nxt = p->next; delete p; p = nxt; }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    std::size_t size() const { return size_; }
    bool empty()       const { return size_ == 0; }

    void push_front(int v) {                       // O(1)
        head_ = new Node{v, head_};
        if (!tail_) tail_ = head_;
        ++size_;
    }
    void push_back(int v) {                        // O(1) thanks to tail_
        Node* n = new Node{v, nullptr};
        if (tail_) tail_->next = n; else head_ = n;
        tail_ = n;
        ++size_;
    }
    void pop_front() {                             // O(1)
        if (!head_) return;
        Node* old = head_;
        head_ = head_->next;
        if (!head_) tail_ = nullptr;
        delete old;
        --size_;
    }

    // insert v AFTER the first node whose value == afterValue (O(n) to find, O(1) to link)
    bool insert_after(int afterValue, int v) {
        for (Node* p = head_; p; p = p->next)
            if (p->value == afterValue) {
                Node* n = new Node{v, p->next};
                p->next = n;
                if (p == tail_) tail_ = n;
                ++size_;
                return true;
            }
        return false;
    }

    // reverse the list in place -- O(n), O(1) extra space (the classic interview question)
    void reverse() {
        Node* prev = nullptr;
        Node* cur  = head_;
        tail_ = head_;
        while (cur) {
            Node* nxt = cur->next;
            cur->next = prev;
            prev = cur;
            cur  = nxt;
        }
        head_ = prev;
    }

    // detect a cycle with Floyd's tortoise-and-hare (used here on our always-acyclic list -> false)
    bool has_cycle() const {
        Node* slow = head_;
        Node* fast = head_;
        while (fast && fast->next) {
            slow = slow->next;
            fast = fast->next->next;
            if (slow == fast) return true;
        }
        return false;
    }

    long long sum() const {
        long long s = 0;
        for (Node* p = head_; p; p = p->next) s += p->value;
        return s;
    }

    void print(const char* label) const {
        std::printf("  %-14s [", label);
        for (Node* p = head_; p; p = p->next) std::printf("%d%s", p->value, p->next ? " " : "");
        std::printf("]  (size %zu)\n", size_);
    }
};

int main() {
    std::printf("=== 1. build + basic ops ===\n");
    IntList l;
    for (int i = 1; i <= 5; ++i) l.push_back(i);   // 1 2 3 4 5
    l.print("push_back x5");
    l.push_front(0);                                // 0 1 2 3 4 5
    l.print("push_front 0");
    l.pop_front();                                  // 1 2 3 4 5
    l.print("pop_front");
    l.insert_after(3, 99);                          // 1 2 3 99 4 5
    l.print("insert_after 3");
    std::printf("  sum = %lld\n", l.sum());

    std::printf("\n=== 2. reverse in place ===\n");
    l.reverse();
    l.print("reversed");
    std::printf("  has_cycle = %s\n", l.has_cycle() ? "yes" : "no");

    std::printf("\n=== 3. why it loses: traverse a list vs a vector (-O2) ===\n");
    {
        const int N = 5'000'000;

        IntList big;
        for (int i = 0; i < N; ++i) big.push_back(i);           // N separate `new Node`
        std::vector<int> vec(static_cast<std::size_t>(N));
        std::iota(vec.begin(), vec.end(), 0);

        volatile long long sink = 0;

        auto t0 = Clock::now();
        sink += big.sum();                                       // pointer-chase, ~1 cache miss per node
        auto t1 = Clock::now();
        { long long s = 0; for (int x : vec) s += x; sink += s; } // contiguous, prefetched, vectorized
        auto t2 = Clock::now();

        auto ms = [](auto x, auto y){ return std::chrono::duration<double, std::milli>(y - x).count(); };
        std::printf("  list  sum : %7.2f ms   (%d heap nodes, scattered)\n", ms(t0, t1), N);
        std::printf("  vector sum: %7.2f ms   (one contiguous buffer)\n",     ms(t1, t2));
        std::printf("  ratio     : %.1fx slower for the list\n", ms(t0, t1) / ms(t1, t2));
        std::printf("  (sink %lld)\n", static_cast<long long>(sink));
    }

    std::printf(
        "\n"
        "  Linked list earns its keep ONLY for: O(1) splice/erase given the node,\n"
        "  iterator/reference stability across inserts, and rare full traversal.\n"
        "  Otherwise std::vector wins -- often by 10-30x -- on cache alone.\n");
    return 0;
}
