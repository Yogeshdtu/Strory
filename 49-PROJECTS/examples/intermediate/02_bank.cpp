// 02_bank.cpp  --  49-PROJECTS intermediate P2
// ============================================================
// Accounts with integer-paise balances. open / deposit / withdraw (no
// overdraft) / transfer (all-or-nothing). Per-account statement. Monthly
// interest with a RATIONAL rate (integer math). A global invariant that
// must hold after any sequence of operations.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 02_bank.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum class Result : std::uint8_t { Ok, NoAccount, Overdraft, BadAmount };

struct Entry {
    std::uint64_t seq;
    std::int64_t  acct;
    std::int64_t  delta_paise;   // +deposit / -withdraw
    std::string   memo;
};

class Bank {
public:
    std::int64_t open(std::int64_t opening_paise = 0) {
        const std::int64_t id = next_acct_++;
        bal_[id] = 0;
        if (opening_paise > 0) (void)deposit(id, opening_paise, "open");
        return id;
    }
    Result deposit(std::int64_t acct, std::int64_t amt, std::string memo = "deposit") {
        if (!bal_.count(acct)) return Result::NoAccount;
        if (amt <= 0)          return Result::BadAmount;
        bal_[acct] += amt;
        total_deposits_ += amt;
        log_.push_back({next_seq_++, acct, amt, std::move(memo)});
        return Result::Ok;
    }
    Result withdraw(std::int64_t acct, std::int64_t amt, std::string memo = "withdraw") {
        const auto it = bal_.find(acct);
        if (it == bal_.end()) return Result::NoAccount;
        if (amt <= 0)         return Result::BadAmount;
        if (it->second < amt) return Result::Overdraft;      // no overdraft
        it->second -= amt;
        total_withdrawals_ += amt;
        log_.push_back({next_seq_++, acct, -amt, std::move(memo)});
        return Result::Ok;
    }
    // Atomic: both legs commit or neither does.
    Result transfer(std::int64_t from, std::int64_t to, std::int64_t amt) {
        if (!bal_.count(from) || !bal_.count(to)) return Result::NoAccount;
        if (amt <= 0)                             return Result::BadAmount;
        if (bal_[from] < amt)                     return Result::Overdraft;  // checked BEFORE any write
        // both checks passed -> safe to apply both legs
        bal_[from] -= amt;
        bal_[to]   += amt;
        log_.push_back({next_seq_++, from, -amt, "xfer-out"});
        log_.push_back({next_seq_++, to,    amt, "xfer-in"});
        return Result::Ok;
    }

    std::int64_t balance(std::int64_t acct) const {
        const auto it = bal_.find(acct);
        return it == bal_.end() ? -1 : it->second;
    }
    std::vector<Entry> statement(std::int64_t acct) const {
        std::vector<Entry> out;
        for (const auto& e : log_) if (e.acct == acct) out.push_back(e);
        return out;
    }

    // Monthly interest: rate = num/den per YEAR, applied as (bal * num) / (den * 12).
    // Integer math throughout -> deterministic, no rounding drift.
    void accrue_monthly_interest(std::int64_t num, std::int64_t den) {
        for (auto& [acct, b] : bal_) {
            const std::int64_t interest = (b * num) / (den * 12);
            if (interest > 0) { b += interest; total_deposits_ += interest;
                                log_.push_back({next_seq_++, acct, interest, "interest"}); }
        }
    }

    // Global invariant: sum of all balances == total deposited - total withdrawn.
    bool invariant_holds() const {
        std::int64_t sum = 0;
        for (const auto& [acct, b] : bal_) { sum += b; (void)acct; }
        return sum == total_deposits_ - total_withdrawals_;
    }

private:
    std::unordered_map<std::int64_t, std::int64_t> bal_;
    std::vector<Entry> log_;
    std::int64_t total_deposits_ = 0;
    std::int64_t total_withdrawals_ = 0;
    std::int64_t next_acct_ = 1;
    std::uint64_t next_seq_ = 1;
};

} // namespace

int main() {
    Bank bank;
    const auto a = bank.open(1000000);   // Rs 10,000.00
    const auto b = bank.open();
    assert(bank.balance(a) == 1000000 && bank.balance(b) == 0);
    assert(bank.invariant_holds());

    assert(bank.withdraw(a, 2000000) == Result::Overdraft);   // rejected
    assert(bank.balance(a) == 1000000);                       // unchanged

    // atomic transfer: both legs or neither
    assert(bank.transfer(a, b, 300000) == Result::Ok);
    assert(bank.balance(a) == 700000 && bank.balance(b) == 300000);
    assert(bank.invariant_holds());

    assert(bank.transfer(b, a, 999999999) == Result::Overdraft);  // fails the precheck
    assert(bank.balance(a) == 700000 && bank.balance(b) == 300000); // NEITHER leg applied
    assert(bank.invariant_holds());

    assert(bank.transfer(a, 9999, 1) == Result::NoAccount);

    // statement is the filtered log
    const auto st = bank.statement(b);
    assert(st.size() == 1 && st.front().delta_paise == 300000);   // just the xfer-in

    // interest: 6%/yr for one month on Rs 7000.00 (700000 paise)
    //   (700000 * 6) / (100 * 12) = 4200000 / 1200 = 3500 paise = Rs 35.00
    bank.accrue_monthly_interest(6, 100);
    assert(bank.balance(a) == 700000 + 3500);
    assert(bank.invariant_holds());

    // property test: 20k random valid-ish ops, invariant holds throughout
    {
        Bank rb;
        std::vector<std::int64_t> ids;
        for (int i = 0; i < 20; ++i) ids.push_back(rb.open(100000));
        std::mt19937 rng(42);
        std::uniform_int_distribution<int>  pick(0, 19);
        std::uniform_int_distribution<int>  amt(1, 5000);
        for (int i = 0; i < 20000; ++i) {
            const auto from = ids[static_cast<std::size_t>(pick(rng))];
            const auto to   = ids[static_cast<std::size_t>(pick(rng))];
            const auto x    = static_cast<std::int64_t>(amt(rng));
            switch (i % 3) {
                case 0: (void)rb.deposit(from, x); break;
                case 1: (void)rb.withdraw(from, x); break;
                default: (void)rb.transfer(from, to, x); break;
            }
            assert(rb.invariant_holds());
        }
    }

    std::puts("02_bank: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - transfer() checks BOTH preconditions before mutating anything -> the two
//     legs are all-or-nothing at the application level. Add a std::scoped_lock
//     over both accounts (sorted by id) for the concurrent version -> the
//     AB/BA deadlock lesson (26).
//   - The global invariant Sum(balances) == deposits - withdrawals is the
//     single highest-value test: run it after every op in a property test.
//   - Interest uses a rational rate + integer math -> no double, no drift (43/14).
// ============================================================
