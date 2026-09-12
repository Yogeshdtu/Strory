// 01_inventory.cpp  --  49-PROJECTS intermediate P1
// ============================================================
// Warehouse inventory: receive / ship / adjust, low-stock report, total
// value (integer paise), an append-only transaction log, and REPLAY
// (rebuild state from the log -> event sourcing / determinism).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 01_inventory.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum class Op   : std::uint8_t { Add, Receive, Ship, Adjust };
enum class Ship : std::uint8_t { Ok, UnknownSku, Insufficient };

struct Txn {
    std::uint64_t seq;
    Op            op;
    std::string   sku;
    std::int64_t  qty;          // signed: Adjust can be negative
    std::int64_t  price_paise;  // meaningful only for Add
};

struct Item {
    std::string  name;
    std::int64_t qty          = 0;
    std::int64_t reorder_level = 0;
    std::int64_t price_paise  = 0;
};

class Inventory {
public:
    bool add(const std::string& sku, std::string name, std::int64_t reorder, std::int64_t price_paise) {
        if (items_.count(sku) || price_paise < 0 || reorder < 0) return false;
        items_[sku] = Item{std::move(name), 0, reorder, price_paise};
        log_.push_back({next_seq_++, Op::Add, sku, 0, price_paise});
        return true;
    }
    bool receive(const std::string& sku, std::int64_t units) {
        auto it = items_.find(sku);
        if (it == items_.end() || units <= 0) return false;
        it->second.qty += units;
        log_.push_back({next_seq_++, Op::Receive, sku, units, 0});
        return true;
    }
    Ship ship(const std::string& sku, std::int64_t units) {
        auto it = items_.find(sku);
        if (it == items_.end())          return Ship::UnknownSku;
        if (units <= 0)                  return Ship::UnknownSku;   // treat as bad request
        if (it->second.qty < units)      return Ship::Insufficient; // reject, no partial ship
        it->second.qty -= units;
        log_.push_back({next_seq_++, Op::Ship, sku, units, 0});
        return Ship::Ok;
    }
    bool adjust(const std::string& sku, std::int64_t delta) {
        auto it = items_.find(sku);
        if (it == items_.end() || it->second.qty + delta < 0) return false;  // invariant: qty >= 0
        it->second.qty += delta;
        log_.push_back({next_seq_++, Op::Adjust, sku, delta, 0});
        return true;
    }

    std::int64_t qty(const std::string& sku) const {
        const auto it = items_.find(sku);
        return it == items_.end() ? -1 : it->second.qty;
    }
    std::int64_t total_value_paise() const {
        std::int64_t v = 0;
        for (const auto& [sku, it] : items_) { v += it.qty * it.price_paise; (void)sku; }
        return v;
    }
    std::vector<std::string> low_stock() const {
        std::vector<std::string> out;
        for (const auto& [sku, it] : items_)
            if (it.qty <= it.reorder_level) out.push_back(sku);
        return out;
    }
    const std::vector<Txn>& log() const { return log_; }

    // Rebuild an Inventory purely from a transaction log.
    static Inventory replay(const std::vector<Txn>& log) {
        Inventory inv;
        for (const auto& t : log) {
            switch (t.op) {
                case Op::Add:     inv.items_[t.sku] = Item{"", 0, 0, t.price_paise}; break;
                case Op::Receive: inv.items_[t.sku].qty += t.qty; break;
                case Op::Ship:    inv.items_[t.sku].qty -= t.qty; break;
                case Op::Adjust:  inv.items_[t.sku].qty += t.qty; break;
            }
        }
        return inv;
    }

private:
    std::unordered_map<std::string, Item> items_;
    std::vector<Txn>                      log_;
    std::uint64_t                         next_seq_ = 1;
};

} // namespace

int main() {
    Inventory inv;
    assert(inv.add("A100", "Widget", /*reorder*/10, /*price*/25000));   // Rs 250.00
    assert(inv.add("B200", "Gadget", 5, 99900));
    assert(!inv.add("A100", "dup", 1, 1));           // duplicate sku
    assert(!inv.add("C300", "neg", 1, -1));          // negative price

    assert(inv.receive("A100", 100));
    assert(inv.qty("A100") == 100);

    assert(inv.ship("A100", 30) == Ship::Ok);
    assert(inv.qty("A100") == 70);

    assert(inv.ship("A100", 1000) == Ship::Insufficient);   // rejected...
    assert(inv.qty("A100") == 70);                          // ...qty unchanged

    assert(inv.ship("ZZZ", 1) == Ship::UnknownSku);

    assert(inv.adjust("A100", -5));                  // shrinkage
    assert(inv.qty("A100") == 65);
    assert(!inv.adjust("A100", -1000));              // would go negative -> rejected
    assert(inv.qty("A100") == 65);

    // total value = 65 * 25000 + 0 * 99900
    assert(inv.total_value_paise() == 65 * 25000);

    // low stock: B200 has qty 0 <= reorder 5
    const auto low = inv.low_stock();
    assert(low.size() == 1 && low.front() == "B200");

    // REPLAY: a fresh inventory built from the log has identical quantities
    const Inventory rebuilt = Inventory::replay(inv.log());
    assert(rebuilt.qty("A100") == 65);
    assert(rebuilt.qty("B200") == 0);
    assert(rebuilt.total_value_paise() == inv.total_value_paise());

    std::puts("01_inventory: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Inventory guards two invariants: qty never negative, price never negative.
//     ship() rejects rather than partially fulfilling.
//   - Every mutation appends a Txn. replay(log) rebuilds the exact state -->
//     this is event sourcing: the log is the source of truth, state is a cache.
//     Same idea as the WAL in intermediate P5 and the deterministic engine in 43/44.
//   - Money is int64_t paise; total value can't drift.
// ============================================================
