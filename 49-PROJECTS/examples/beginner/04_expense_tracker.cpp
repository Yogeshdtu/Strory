// 04_expense_tracker.cpp  --  49-PROJECTS beginner P4
// ============================================================
// Expense records with INTEGER money (paise). Totals, per-category totals,
// monthly filter, top-3 categories, per-category budget warnings.
// double for money is a bug -> everything is int64_t paise.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 04_expense_tracker.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

struct Expense {
    int          yyyymm;        // e.g. 202603
    std::string  category;
    std::int64_t paise;         // 12345 == Rs 123.45
    std::string  note;
};

class Tracker {
public:
    void add(int yyyymm, std::string category, std::int64_t paise, std::string note = {}) {
        items_.push_back({yyyymm, std::move(category), paise, std::move(note)});
    }

    std::int64_t total() const {
        std::int64_t t = 0;
        for (const auto& e : items_) t += e.paise;
        return t;
    }
    std::int64_t total_for_month(int yyyymm) const {
        std::int64_t t = 0;
        for (const auto& e : items_) if (e.yyyymm == yyyymm) t += e.paise;
        return t;
    }
    std::map<std::string, std::int64_t> by_category() const {
        std::map<std::string, std::int64_t> m;
        for (const auto& e : items_) m[e.category] += e.paise;
        return m;
    }
    // The N highest-spend categories, descending. Uses partial_sort, not a full sort.
    std::vector<std::pair<std::string, std::int64_t>> top_categories(std::size_t n) const {
        const auto cat = by_category();
        std::vector<std::pair<std::string, std::int64_t>> v(cat.begin(), cat.end());
        if (n > v.size()) n = v.size();
        std::partial_sort(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n), v.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
        v.resize(n);
        return v;
    }
    // Categories whose total exceeds their budget, each reported once.
    std::vector<std::string> over_budget(const std::map<std::string, std::int64_t>& budgets) const {
        std::vector<std::string> out;
        const auto cat = by_category();
        for (const auto& [name, limit] : budgets) {
            const auto it = cat.find(name);
            if (it != cat.end() && it->second > limit) out.push_back(name);
        }
        return out;
    }

private:
    std::vector<Expense> items_;
};

} // namespace

int main() {
    Tracker t;
    t.add(202603, "food",      12000);      // Rs 120.00
    t.add(202603, "transport", 4550);       // Rs  45.50
    t.add(202603, "food",      8000);       // Rs  80.00
    t.add(202604, "rent",      1500000);    // Rs 15000.00
    t.add(202604, "food",      6000);

    // integer money round-trips exactly (no 0.1 + 0.2 problem)
    assert(t.total() == 12000 + 4550 + 8000 + 1500000 + 6000);

    assert(t.total_for_month(202603) == 12000 + 4550 + 8000);
    assert(t.total_for_month(202604) == 1500000 + 6000);
    assert(t.total_for_month(209912) == 0);

    const auto cat = t.by_category();
    assert(cat.at("food") == 12000 + 8000 + 6000);
    assert(cat.at("transport") == 4550);
    // category totals sum to the grand total
    std::int64_t sum = 0;
    for (const auto& [k, v] : cat) { sum += v; (void)k; }
    assert(sum == t.total());

    const auto top = t.top_categories(3);
    assert(top.size() == 3);
    assert(top[0].first == "rent");        // 1500000
    assert(top[1].first == "food");        // 26000
    assert(top[2].first == "transport");   // 4550
    assert(t.top_categories(99).size() == 3);   // clamped

    const auto warn = t.over_budget({{"food", 20000}, {"transport", 10000}, {"rent", 2000000}});
    assert(warn.size() == 1 && warn.front() == "food");   // only food is over

    std::puts("04_expense_tracker: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Money is int64_t paise. Never double: 0.1 has no exact binary form, and
//     summing thousands of them drifts. Format for display only (paise/100).
//   - by_category returns a std::map (ordered) so reports are deterministic.
//   - top_categories uses std::partial_sort (O(m log n)), not std::sort -- you
//     only need the top few sorted (19/20).
//   - Date as int yyyymm keeps comparison/filtering trivial; std::chrono for
//     real calendar math (22).
// ============================================================
