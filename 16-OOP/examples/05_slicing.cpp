// 05_slicing.cpp
// ============================================================
// Object slicing -- Derived ko Base BY VALUE mein daalo -> Derived part "kat" jaata
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_slicing.cpp -o sl && ./sl
// ============================================================
//   Base b = derived;         // ⚠️ SLICE -- sirf Base subobject copy hota, vptr = Base ki
//   void f(Base b);            // ⚠️ SLICE -- by-value parameter
//   container<Base>            // ⚠️ SLICE -- elements Base-sized, Derived nahi samaata
//
//   Fix: base ko POINTER ya REFERENCE se rakho (Base*, Base&, unique_ptr<Base>).
// ============================================================

#include <iostream>
#include <memory>
#include <vector>

struct Employee {
    std::string name_;
    explicit Employee(std::string n) : name_(std::move(n)) {}
    virtual ~Employee() = default;
    virtual double monthlyPay() const { return 30000.0; }
    virtual const char* role() const { return "Employee"; }
};

struct Manager : Employee {
    double bonus_;
    Manager(std::string n, double bonus) : Employee(std::move(n)), bonus_(bonus) {}
    double monthlyPay() const override { return 80000.0 + bonus_; }
    const char* role() const override { return "Manager"; }
};

void printPayByValue(Employee e) {                 // ⚠️ BY VALUE -> slices
    std::cout << "  [by value] " << e.name_ << " (" << e.role()
              << ") pay = " << e.monthlyPay() << "\n";
}

void printPayByRef(const Employee& e) {            // ✅ BY REFERENCE -> polymorphic
    std::cout << "  [by ref]   " << e.name_ << " (" << e.role()
              << ") pay = " << e.monthlyPay() << "\n";
}

int main() {
    Manager m{"Asha", 20000.0};

    std::cout << "=== direct call on Manager ===\n";
    std::cout << "  " << m.role() << " pay = " << m.monthlyPay() << "\n";   // Manager, 100000

    std::cout << "\n=== pass by value -- SLICED ===\n";
    printPayByValue(m);            // Employee copy banti -> role()="Employee", pay=30000

    std::cout << "\n=== pass by reference -- polymorphic ===\n";
    printPayByRef(m);             // Manager, 100000

    std::cout << "\n=== assignment slice ===\n";
    Employee sliced = m;          // ⚠️ sirf Employee subobject copy; bonus_ gaya; vptr=Employee's
    std::cout << "  sliced.role() = " << sliced.role()
              << ", pay = " << sliced.monthlyPay() << "   (Manager-ness gayab)\n";

    std::cout << "\n=== container of Base (by value) -- SLICES on insert ===\n";
    std::vector<Employee> byValue;
    byValue.push_back(m);                          // ⚠️ Manager -> Employee slice
    std::cout << "  byValue[0].role() = " << byValue[0].role()
              << ", pay = " << byValue[0].monthlyPay() << "\n";

    std::cout << "\n=== container of pointers -- NO slice ===\n";
    std::vector<std::unique_ptr<Employee>> byPtr;
    byPtr.push_back(std::make_unique<Manager>("Ravi", 15000.0));
    byPtr.push_back(std::make_unique<Employee>("Sam"));
    for (const auto& e : byPtr)
        std::cout << "  " << e->name_ << " (" << e->role() << ") pay = " << e->monthlyPay() << "\n";

    std::cout <<
        "\n"
        "  Slice: Derived -> Base by value = Derived part copy nahi hota, vptr Base ki set\n"
        "  Polymorphism ke liye base ko Base& / Base* / unique_ptr<Base> se rakho\n"
        "  vector<Base> ❌   ->   vector<unique_ptr<Base>> ✅  (ya vector<Base*> agar ownership alag)\n";
    return 0;
}
