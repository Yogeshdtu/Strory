// 05_bst.cpp
// ============================================================
// Binary search tree -- insert, find, delete (3 cases), the four
// traversals, height, and a demo of WHY an unbalanced BST degrades
// to a linked list (sorted-insert -> O(n) height).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_bst.cpp -o bst && ./bst
// ============================================================

#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

class BST {
    struct Node {
        int   key;
        Node* left  = nullptr;
        Node* right = nullptr;
    };
    Node*       root_ = nullptr;
    std::size_t size_ = 0;

    static void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }
    static Node* minNode(Node* n) { while (n->left) n = n->left; return n; }

    static int heightRec(const Node* n) {
        if (!n) return 0;
        int l = heightRec(n->left), r = heightRec(n->right);
        return 1 + (l > r ? l : r);
    }

public:
    BST() = default;
    BST(const BST&)            = delete;
    BST& operator=(const BST&) = delete;
    ~BST() { destroy(root_); }

    std::size_t size() const { return size_; }
    int  height()      const { return heightRec(root_); }

    void insert(int key) {
        Node** cur = &root_;
        while (*cur) {
            if      (key < (*cur)->key) cur = &(*cur)->left;
            else if (key > (*cur)->key) cur = &(*cur)->right;
            else                        return;             // no duplicates
        }
        *cur = new Node{key};
        ++size_;
    }

    bool find(int key) const {
        const Node* n = root_;
        while (n) {
            if      (key == n->key) return true;
            else if (key <  n->key) n = n->left;
            else                    n = n->right;
        }
        return false;
    }

    void erase(int key) {
        Node** cur = &root_;
        while (*cur && (*cur)->key != key)
            cur = (key < (*cur)->key) ? &(*cur)->left : &(*cur)->right;
        if (!*cur) return;                                  // not found

        Node* target = *cur;
        if (!target->left) {                               // 0 or 1 child (right)
            *cur = target->right;
        } else if (!target->right) {                       // 1 child (left)
            *cur = target->left;
        } else {                                           // 2 children: replace with in-order successor
            Node** succ = &target->right;
            while ((*succ)->left) succ = &(*succ)->left;
            Node* s = *succ;
            *succ = s->right;                              // unlink successor
            s->left  = target->left;
            s->right = target->right;
            *cur = s;
        }
        delete target;
        --size_;
    }

    template <class F>
    void inorder(F visit) const {
        std::function<void(const Node*)> go = [&](const Node* n) {
            if (!n) return;
            go(n->left); visit(n->key); go(n->right);
        };
        go(root_);
    }

    void printAllTraversals() const {
        auto dump = [](const char* name, const std::vector<int>& v) {
            std::printf("  %-10s ", name);
            for (int x : v) std::printf("%d ", x);
            std::printf("\n");
        };
        std::vector<int> pre, in, post, level;
        std::function<void(const Node*)> P = [&](const Node* n){ if(!n) return; pre.push_back(n->key);  P(n->left); P(n->right); };
        std::function<void(const Node*)> I = [&](const Node* n){ if(!n) return; I(n->left); in.push_back(n->key); I(n->right); };
        std::function<void(const Node*)> O = [&](const Node* n){ if(!n) return; O(n->left); O(n->right); post.push_back(n->key); };
        P(root_); I(root_); O(root_);
        std::vector<const Node*> q{root_};
        for (std::size_t i = 0; i < q.size(); ++i) {
            const Node* n = q[i];
            if (!n) continue;
            level.push_back(n->key);
            if (n->left)  q.push_back(n->left);
            if (n->right) q.push_back(n->right);
        }
        dump("preorder",  pre);
        dump("inorder",   in);          // BST inorder == sorted
        dump("postorder", post);
        dump("levelorder",level);
    }
};

int main() {
    std::printf("=== 1. build a balanced-ish BST ===\n");
    BST t;
    for (int k : {50, 30, 70, 20, 40, 60, 80, 35, 45}) t.insert(k);
    std::printf("  size=%zu  height=%d\n", t.size(), t.height());
    t.printAllTraversals();

    std::printf("\n=== 2. find ===\n");
    for (int k : {35, 55, 80, 100})
        std::printf("  find(%3d) = %s\n", k, t.find(k) ? "yes" : "no");

    std::printf("\n=== 3. erase (all three cases) ===\n");
    t.erase(20);   // leaf
    t.erase(70);   // two children -> successor is 80
    t.erase(30);   // one/other child arrangement
    std::printf("  after erase 20,70,30 -> inorder: ");
    t.inorder([](int k){ std::printf("%d ", k); });
    std::printf("\n  size=%zu height=%d\n", t.size(), t.height());

    std::printf("\n=== 4. unbalanced: sorted insert -> O(n) height ===\n");
    {
        BST bal, deg;
        for (int i = 1; i <= 1023; ++i) deg.insert(i);          // ascending -> right-spine
        // balanced insertion order: middle-out
        std::function<void(int,int)> ins = [&](int lo, int hi){
            if (lo > hi) return;
            int mid = lo + (hi - lo) / 2;
            bal.insert(mid);
            ins(lo, mid - 1);
            ins(mid + 1, hi);
        };
        ins(1, 1023);
        std::printf("  1023 ascending inserts : height = %4d   (~n, a linked list)\n", deg.height());
        std::printf("  1023 middle-out inserts: height = %4d   (~log2(1024) = 10)\n", bal.height());
        std::printf("  -> a plain BST has NO self-balancing; std::map uses a red-black tree.\n");
    }
    return 0;
}
