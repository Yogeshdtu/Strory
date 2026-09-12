// 08_order_class.cpp
// ============================================================
// HFT-style Order class -- encapsulation + invariant + state machine
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 08_order_class.cpp -o oc && ./oc
// ============================================================
//   Ek Order ke rules (invariants):
//   - qty > 0, price > 0  (construction pe check)
//   - filledQty kabhi qty se zyada nahi
//   - state transitions: New -> (PartiallyFilled ->)* Filled | Cancelled
//   - Cancelled / Filled ke baad koi change nahi
//
//   Yeh sab class enforce karti hai -- bahar wala invalid Order bana hi nahi sakta.
//   (Hot-path note: yeh POD-ish, trivially copyable rehna chahiye -- static_assert.)
// ============================================================

#include <cstdint>
#include <iostream>
#include <type_traits>

enum class Side : std::uint8_t { Buy, Sell };
enum class OrderState : std::uint8_t { New, PartiallyFilled, Filled, Cancelled };

class Order {
    std::uint64_t id_        = 0;
    std::int64_t  priceTicks_ = 0;      // integer ticks -- no float in hot path
    std::uint32_t qty_       = 0;
    std::uint32_t filledQty_ = 0;
    Side          side_      = Side::Buy;
    OrderState    state_     = OrderState::New;

public:
    Order() = default;

    Order(std::uint64_t id, Side side, std::int64_t priceTicks, std::uint32_t qty)
        : id_(id), priceTicks_(priceTicks), qty_(qty), side_(side) {
        // invariant: construction pe hi valid banao ya "New with qty 0" reject-able
        if (priceTicks <= 0 || qty == 0) {
            state_ = OrderState::Cancelled;   // simple demo: invalid -> born dead
        }
    }

    // ---- const accessors ----
    std::uint64_t id()         const { return id_; }
    Side          side()       const { return side_; }
    std::int64_t  priceTicks() const { return priceTicks_; }
    std::uint32_t qty()        const { return qty_; }
    std::uint32_t filledQty()  const { return filledQty_; }
    std::uint32_t leavesQty()  const { return qty_ - filledQty_; }
    OrderState    state()      const { return state_; }
    bool          isActive()   const {
        return state_ == OrderState::New || state_ == OrderState::PartiallyFilled;
    }

    // ---- mutating ops -- invariants ke andar ----
    bool fill(std::uint32_t execQty) {
        if (!isActive() || execQty == 0 || execQty > leavesQty()) return false;
        filledQty_ += execQty;
        state_ = (filledQty_ == qty_) ? OrderState::Filled : OrderState::PartiallyFilled;
        return true;
    }

    bool cancel() {
        if (!isActive()) return false;           // Filled/Cancelled se cancel nahi
        state_ = OrderState::Cancelled;
        return true;
    }
};

// hot-path requirement: Order ko bina ceremony ke copy / pool mein rakh sakein
static_assert(std::is_trivially_copyable_v<Order>, "Order must stay trivially copyable");
static_assert(sizeof(Order) == 32, "Order layout changed -- check padding");

static const char* toStr(OrderState s) {
    switch (s) {
        case OrderState::New:             return "New";
        case OrderState::PartiallyFilled: return "PartiallyFilled";
        case OrderState::Filled:          return "Filled";
        case OrderState::Cancelled:       return "Cancelled";
    }
    return "?";
}

static void dump(const Order& o) {
    std::cout << "  Order#" << o.id()
              << (o.side() == Side::Buy ? " BUY  " : " SELL ")
              << o.qty() << " @ " << o.priceTicks()
              << "  filled=" << o.filledQty() << " leaves=" << o.leavesQty()
              << "  [" << toStr(o.state()) << "]\n";
}

int main() {
    std::cout << "sizeof(Order) = " << sizeof(Order) << " bytes\n\n";

    Order o{1001, Side::Buy, 15025, 100};
    dump(o);

    std::cout << "fill(40)  -> " << std::boolalpha << o.fill(40) << "\n";  dump(o);
    std::cout << "fill(100) -> " << o.fill(100) << "  (only 60 leaves -> reject)\n"; dump(o);
    std::cout << "fill(60)  -> " << o.fill(60) << "\n"; dump(o);
    std::cout << "cancel()  -> " << o.cancel() << "  (already Filled -> reject)\n"; dump(o);

    std::cout << "\n";
    Order bad{1002, Side::Sell, -5, 10};        // invalid price
    std::cout << "invalid price order: "; dump(bad);

    Order c{1003, Side::Sell, 15030, 50};
    std::cout << "\ncancel active order -> " << c.cancel() << "\n"; dump(c);
    std::cout << "fill after cancel   -> " << c.fill(10) << "  (rejected)\n";

    std::cout <<
        "\n"
        "  Bahar se Order ke fields likhe NAHI ja sakte (private).\n"
        "  qty/price rules ctor mein, fill/cancel rules methods mein.\n"
        "  static_assert: trivially-copyable + fixed 32B -> pool/memcpy safe (folder 14).\n";
    return 0;
}
