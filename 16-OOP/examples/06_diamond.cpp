// 06_diamond.cpp
// ============================================================
// Diamond problem -- multiple inheritance -> duplicate base -> virtual inheritance fix
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_diamond.cpp -o dm && ./dm
// ============================================================
//              Device
//             .      .
//        Scanner      Printer      (dono Device se derive)
//             .      .
//           Photocopier            (Scanner + Printer dono se)
//
//   Non-virtual: Photocopier ke andar 2 Device subobjects (Scanner ka, Printer ka)
//                -> ambiguity, double state, kaunsa serial number?
//   Virtual    : Scanner/Printer `virtual public Device` -> Photocopier mein
//                EK shared Device subobject
// ============================================================

#include <iostream>
#include <string>

// ---------- NON-VIRTUAL diamond (problem) ----------
namespace bad {
struct Device {
    std::string serial_;
    explicit Device(std::string s) : serial_(std::move(s)) {
        std::cout << "  bad::Device(" << serial_ << ")\n";
    }
    void status() const { std::cout << "  Device " << serial_ << " ok\n"; }
};
struct Scanner : Device {
    Scanner() : Device("SCN-1") {}
};
struct Printer : Device {
    Printer() : Device("PRN-1") {}
};
struct Photocopier : Scanner, Printer {         // 2 Device subobjects!
    Photocopier() { std::cout << "  bad::Photocopier ctor\n"; }
};
}

// ---------- VIRTUAL inheritance (fix) ----------
namespace good {
struct Device {
    std::string serial_;
    explicit Device(std::string s) : serial_(std::move(s)) {
        std::cout << "  good::Device(" << serial_ << ")\n";
    }
    void status() const { std::cout << "  Device " << serial_ << " ok\n"; }
};
struct Scanner : virtual Device {               // virtual -> shared Device
    Scanner() : Device("unused-here") {}        // ignored jab Photocopier banega
};
struct Printer : virtual Device {
    Printer() : Device("unused-here") {}
};
struct Photocopier : Scanner, Printer {
    // MOST-DERIVED class virtual base ko DIRECTLY init karta hai
    Photocopier() : Device("COPY-42") {
        std::cout << "  good::Photocopier ctor\n";
    }
};
}

int main() {
    std::cout << "=== NON-virtual diamond: 2 Device subobjects ===\n";
    bad::Photocopier bp;
    // bp.serial_;         // ❌ ERROR: ambiguous -- Scanner::Device::serial_ ya Printer::Device::serial_?
    // bp.status();        // ❌ ERROR: ambiguous
    std::cout << "  scanner side serial: " << static_cast<bad::Scanner&>(bp).serial_ << "\n";
    std::cout << "  printer side serial: " << static_cast<bad::Printer&>(bp).serial_ << "\n";
    std::cout << "  sizeof(bad::Photocopier) = " << sizeof(bad::Photocopier)
              << "   (2 x Device)\n";

    std::cout << "\n=== VIRTUAL inheritance: 1 shared Device ===\n";
    good::Photocopier gp;
    std::cout << "  gp.serial_ = " << gp.serial_ << "   (unambiguous -- ek hi Device)\n";
    gp.status();                                 // unambiguous
    static_cast<good::Scanner&>(gp).status();    // same Device
    static_cast<good::Printer&>(gp).status();    // same Device
    std::cout << "  sizeof(good::Photocopier) = " << sizeof(good::Photocopier)
              << "   (1 x Device + vbase pointers -- virtual inheritance ka overhead)\n";

    std::cout <<
        "\n"
        "  Multiple inheritance of interfaces (pure-virtual, no state) -> usually fine.\n"
        "  Diamond WITH STATE -> virtual inheritance chahiye (ek shared base).\n"
        "  Cost: extra vbase pointers, non-constant base offset, most-derived\n"
        "  class ko virtual base init karna padta. HFT: multiple inheritance +\n"
        "  virtual bases hot path pe avoid -- composition ya single-inheritance.\n";
    return 0;
}
