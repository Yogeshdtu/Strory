// 05_use_after_move.cpp  --  ismein EK bug hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 05_use_after_move.cpp -o t && ./t
// Expected: "queued 3 orders, log has 3 lines"   Actual: "log has 0 lines".
// ============================================================
#include <cstdio>
#include <string>
#include <vector>

struct Order { std::string sym; int qty; };

class OrderQueue {
public:
    void push(Order o) {
        audit_log_.push_back(o.sym + " x" + std::to_string(o.qty));
        pending_.push_back(std::move(o));      // <-- o ka guts move ho gaya
        last_symbol_ = o.sym;                  // <-- dekho yahan: moved-from o padha
    }
    std::size_t log_lines() const { return audit_log_.size(); }
    const std::string& last_symbol() const { return last_symbol_; }
private:
    std::vector<Order>       pending_;
    std::vector<std::string> audit_log_;
    std::string             last_symbol_;
};

int main() {
    OrderQueue q;
    q.push({"AAPL", 10});
    q.push({"MSFT", 5});
    q.push({"GOOG", 2});
    std::printf("queued 3 orders, log has %zu lines, last='%s'\n",
                q.log_lines(), q.last_symbol().c_str());
    return 0;
}

// ============================================================
// BUG:     `pending_.push_back(std::move(o))` ke baad `o` "moved-from" hai
//          -- valid par UNSPECIFIED state. `o.sym` ab aksar khaali string.
//          `last_symbol_ = o.sym;` isliye "" set karta.
//          (Yahan crash nahi, sirf GALAT data -- worse, kyunki chup hai.
//          `log_lines()` theek hai; asli galti `last_symbol_` mein.)
// SYMPTOM: `last='` khaali. Agar `push` ka order/timing badla to kabhi
//          purani value dikh sakti -- "unspecified" ka matlab yahi.
// TOOL:    clang-tidy `bugprone-use-after-move`, ya Clang
//          `-Wpessimizing-move`/static analyzers. GCC `-Wall` ise NAHI
//          pakadta. Code review + rule: `std::move` ke baad us object ko
//          sirf assign ya destroy karo, PADHO mat.
// FIX:     Move se PEHLE jo chahiye woh nikaal lo:
//            last_symbol_ = o.sym;                 // pehle
//            pending_.push_back(std::move(o));     // phir move
//          Ya move hi mat karo agar `o` aage chahiye (copy le lo).
//          (18-COPY-MOVE, 22-MODERN-CPP/move-semantics).
// ============================================================
