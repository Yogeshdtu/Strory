// 01_first_class.cpp
// ============================================================
// Pehli class -- private data + public interface + `this`
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_first_class.cpp -o fc && ./fc
// ============================================================
//   struct = data ka bundle.
//   class  = data + us data pe kaam karne wale functions + ACCESS CONTROL.
//
//   Idea: data ko `private` rakho, us tak pahunchne ke liye `public` methods do.
//   Isse ek INVARIANT (hamesha-sach rehne wala rule) enforce hota hai --
//   yahan: balance kabhi negative nahi hoga.
// ============================================================

#include <iostream>
#include <string>

class BankAccount {
    // ---- private: sirf class ke apne methods chhoo sakte hain ----
    std::string owner_;
    long long   balanceCents_ = 0;      // paisa cents mein (float se bachne ke liye)

public:
    // ---- constructor: object bante hi invariant set karo ----
    BankAccount(std::string owner, long long openingCents)
        : owner_(std::move(owner)), balanceCents_(openingCents < 0 ? 0 : openingCents) {}

    // ---- public interface: bahar wale sirf yeh use kar sakte hain ----
    void deposit(long long cents) {
        if (cents <= 0) return;                 // guard -- galat input ignore
        balanceCents_ += cents;
    }

    bool withdraw(long long cents) {
        if (cents <= 0 || cents > balanceCents_) return false;   // invariant protect
        balanceCents_ -= cents;
        return true;
    }

    // ---- const method: object ko padhta hai, badalta nahi (file 07) ----
    long long balanceCents() const { return balanceCents_; }
    const std::string& owner() const { return owner_; }

    void print() const {
        // `this` -> current object ka pointer. `this->owner_` == `owner_`.
        std::cout << "  " << this->owner_ << " : "
                  << balanceCents_ / 100 << "." << (balanceCents_ % 100 < 10 ? "0" : "")
                  << balanceCents_ % 100 << "\n";
    }
};

int main() {
    BankAccount acc{"Asha", 5000};             // 50.00
    acc.print();

    acc.deposit(2550);                          // +25.50
    acc.print();

    std::cout << "  withdraw 100.00 -> " << (acc.withdraw(10000) ? "ok" : "DENIED (invariant)") << "\n";
    std::cout << "  withdraw 60.00  -> " << (acc.withdraw(6000)  ? "ok" : "DENIED") << "\n";
    acc.print();

    // ---- encapsulation: yeh compile NAHI hote ----
    // acc.balanceCents_ = 999999;   // ❌ private
    // acc.owner_ = "Chor";          // ❌ private
    // Bahar se balance sirf deposit/withdraw ke rules se badal sakta hai.

    std::cout << "\n  final balance (cents): " << acc.balanceCents() << "\n";
    std::cout << "  owner: " << acc.owner() << "\n";

    std::cout <<
        "\n"
        "  class BankAccount {\n"
        "      long long balanceCents_;   // private -- protected data\n"
        "  public:\n"
        "      void deposit(...);         // public -- controlled access\n"
        "      bool withdraw(...);        // enforces: balance >= 0\n"
        "  };\n"
        "  Invariant (balance kabhi negative nahi) class ke andar hi enforce hota hai.\n";
    return 0;
}
