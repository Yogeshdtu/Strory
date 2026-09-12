// 01_reverse_linked_list.cpp
// ============================================================
// CLASSIC: singly linked list ko in-place reverse karo.
// Iterative (O(1) space) aur recursive dono. Plus: is it worth
// discussing that a real HFT codebase rarely uses linked lists?
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 01_reverse_linked_list.cpp -o t && ./t
// ============================================================
// INTERVIEWER KYA DEKH RAHA:
//   - pointer manipulation without leaks / dangling
//   - the 3-pointer iterative dance (prev/cur/next) done cleanly
//   - edge cases: empty list, single node
//   - can you also do it recursively and state the stack cost
//   - bonus: "when would you NOT use a linked list?" -> cache locality,
//     per-node alloc; HFT uses flat arrays / intrusive lists in a pool
// ============================================================

#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

struct Node {
    int   val;
    Node* next = nullptr;
    explicit Node(int v) : val(v) {}
};

// O(n) time, O(1) space. Standard prev/cur/next.
static Node* reverse_iterative(Node* head) {
    Node* prev = nullptr;
    Node* cur  = head;
    while (cur != nullptr) {
        Node* next = cur->next;   // save
        cur->next  = prev;        // flip
        prev       = cur;         // advance prev
        cur        = next;        // advance cur
    }
    return prev;                  // new head
}

// O(n) time, O(n) stack. Reverses the tail, then fixes up the current link.
static Node* reverse_recursive(Node* head) {
    if (head == nullptr || head->next == nullptr) return head;
    Node* new_head = reverse_recursive(head->next);
    head->next->next = head;      // (a -> b) becomes (b -> a)
    head->next = nullptr;
    return new_head;
}

// ---- test helpers -----------------------------------------------------
static Node* build(const std::vector<int>& xs) {
    Node* head = nullptr;
    Node* tail = nullptr;
    for (int x : xs) {
        Node* n = new Node(x);
        if (!head) head = tail = n;
        else { tail->next = n; tail = n; }
    }
    return head;
}
static std::vector<int> to_vec(Node* head) {
    std::vector<int> out;
    for (Node* p = head; p != nullptr; p = p->next) out.push_back(p->val);
    return out;
}
static void destroy(Node* head) {
    while (head != nullptr) { Node* n = head->next; delete head; head = n; }
}

int main() {
    // empty
    assert(reverse_iterative(nullptr) == nullptr);
    assert(reverse_recursive(nullptr) == nullptr);

    // single
    {
        Node* h = build({7});
        h = reverse_iterative(h);
        assert((to_vec(h) == std::vector<int>{7}));
        destroy(h);
    }

    // general -- iterative
    {
        Node* h = build({1, 2, 3, 4, 5});
        h = reverse_iterative(h);
        assert((to_vec(h) == std::vector<int>{5, 4, 3, 2, 1}));
        destroy(h);
    }

    // general -- recursive
    {
        Node* h = build({1, 2, 3, 4, 5});
        h = reverse_recursive(h);
        assert((to_vec(h) == std::vector<int>{5, 4, 3, 2, 1}));
        destroy(h);
    }

    // reverse twice == identity
    {
        Node* h = build({9, 8, 7, 6});
        h = reverse_iterative(h);
        h = reverse_iterative(h);
        assert((to_vec(h) == std::vector<int>{9, 8, 7, 6}));
        destroy(h);
    }

    std::printf("01_reverse_linked_list: ALL PASS\n");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - iterative: O(n) time, O(1) space -- preferred.
//   - recursive: O(n) time, O(n) stack -- risk of stack overflow on a
//     long list (adversarial input). Mention it.
//   - "no leaks": every `new` has a matching `delete` in the test.
//   - HFT aside: linked lists = per-node heap alloc + pointer chase (can't
//     be pipelined). Real low-latency code uses a flat array or an
//     INTRUSIVE list whose nodes live in a pre-allocated pool (the `next`
//     index/pointer is a member of the payload). (folders 20, 32, 36, 44)
// ============================================================
