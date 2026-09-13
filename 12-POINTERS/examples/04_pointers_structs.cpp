// 04_pointers_structs.cpp
// ============================================================
// Struct ke pointers -- -> operator
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_pointers_structs.cpp -o ps && ./ps
// ============================================================
//   p->member   ==   (*p).member
// Dono same hain; -> zyada readable hai (aur idiomatic).
// ============================================================

#include <cstdint>
#include <iostream>
#include <string>

struct Order {
    std::uint64_t id;
    std::string   symbol;
    double        price;
    std::int64_t  qty;
};

// function ko struct ka POINTER do -> caller ka original modify hota hai, koi copy nahi
void applyFill(Order* o, std::int64_t fillQty) {
    if (o == nullptr) return;             // ⚠️ pointer null ho sakta hai -- check
    o->qty -= fillQty;                    // (*o).qty -= fillQty;  ke barabar
    if (o->qty < 0) o->qty = 0;
}

// read-only -> const Order*
void printOrder(const Order* o) {
    if (!o) { std::cout << "  (null)\n"; return; }
    std::cout << "  #" << o->id << "  " << o->symbol
              << "  " << o->qty << " @ " << o->price << "\n";
    // o->qty = 0;   // ❌ ERROR -- const Order*
}

int main() {
    Order ord{1001, "AAPL", 192.34, 100};

    // ============================================================
    //  1. Struct ka pointer, -> se access
    // ============================================================
    std::cout << "===== 1. -> operator =====\n";
    Order* p = &ord;
    std::cout << "  p->id      = " << p->id << "\n";
    std::cout << "  (*p).price = " << (*p).price << "   <- same as p->price\n";
    std::cout << "  p->symbol  = " << p->symbol << "\n";

    p->price = 193.00;                    // pointer ke through badlo -- asli `ord` badla
    p->qty  += 50;
    std::cout << "  after edit: " << p->qty << " @ " << p->price << "\n";

    // ============================================================
    //  2. Function ko struct ka pointer do
    // ============================================================
    std::cout << "\n===== 2. modify via pointer parameter =====\n";
    printOrder(&ord);
    applyFill(&ord, 60);
    std::cout << "  after applyFill(&ord, 60):\n";
    printOrder(&ord);

    // ============================================================
    //  3. nullptr -- pointer shayad kahin point hi na kare
    // ============================================================
    std::cout << "\n===== 3. nullptr =====\n";
    Order* maybe = nullptr;
    std::cout << "  printOrder(nullptr): ";
    printOrder(maybe);
    std::cout << "  applyFill(nullptr, 10): ";
    applyFill(maybe, 10);
    std::cout << "handled safely (guard clause)\n";
    // maybe->id;    // 💥 null dereference -> crash

    // ============================================================
    //  4. Chain: aise struct ka pointer jiska member khud pointer hai
    // ============================================================
    std::cout << "\n===== 4. chained -> =====\n";
    struct Node { int value; Node* next; };
    Node n3{3, nullptr};
    Node n2{2, &n3};
    Node n1{1, &n2};

    Node* cur = &n1;
    std::cout << "  list: ";
    while (cur != nullptr) {              // classic linked-list walk -- nullptr = list khatam
        std::cout << cur->value << " ";
        cur = cur->next;                 // "agle node pe jao"
    }
    std::cout << "\n  n1.next->next->value = " << n1.next->next->value << "   (== 3)\n";

    std::cout <<
        "\n"
        "  p->x  ==  (*p).x       -- prefer ->\n"
        "  const Order* -> read-only; Order* -> can modify\n"
        "  Always null-check a pointer parameter before ->\n"
        "  Linked structures (lists, trees) = structs pointing to structs\n";

    return 0;
}
