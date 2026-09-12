// 04_manipulators.cpp
// ============================================================
// <iomanip> ka poora tour -- aur sticky behaviour ke traps
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_manipulators.cpp -o manip && ./manip
// ============================================================

#include <iostream>
#include <iomanip>
#include <ios>
#include <string>
#include <cstdint>

// ============================================================
//  RAII helper: stream ki formatting state save/restore karta hai
//  Manipulators STICKY hote hain, isliye function se nikalte waqt
//  state restore karna achhi practice hai.
//  (RAII folder 17 mein poora -- yahan bas pattern dekho.)
// ============================================================
class IosStateSaver {
    std::ios&          stream_;
    std::ios::fmtflags flags_;
    std::streamsize    precision_;
    char               fill_;
public:
    explicit IosStateSaver(std::ios& s)
        : stream_(s), flags_(s.flags()),
          precision_(s.precision()), fill_(s.fill()) {}

    ~IosStateSaver() {                       // destructor -> automatic restore
        stream_.flags(flags_);
        stream_.precision(precision_);
        stream_.fill(fill_);
    }

    IosStateSaver(const IosStateSaver&)            = delete;
    IosStateSaver& operator=(const IosStateSaver&) = delete;
};

int main() {
    // ============================================================
    //  1. ⚠️ setw STICKY NAHI HAI -- sirf agli value pe lagta hai
    // ============================================================
    std::cout << "===== 1. setw STICKY NAHI HAI =====\n";
    std::cout << "setw(8) << 1 << 2 << 3  -> [";
    std::cout << std::setw(8) << 1 << 2 << 3;
    std::cout << "]\n";
    std::cout << "  ^ setw sirf `1` pe laga, 2 aur 3 pe nahi\n";

    std::cout << "Har value pe setw          -> [";
    std::cout << std::setw(8) << 1 << std::setw(8) << 2 << std::setw(8) << 3;
    std::cout << "]\n";

    // ============================================================
    //  2. ALIGNMENT
    // ============================================================
    std::cout << "\n===== 2. ALIGNMENT =====\n";
    std::cout << "right (default): [" << std::setw(10) << 42 << "]\n";
    std::cout << "left:            [" << std::left  << std::setw(10) << 42 << "]\n";
    std::cout << "right:           [" << std::right << std::setw(10) << 42 << "]\n";
    std::cout << "internal:        [" << std::internal << std::setw(10) << -42 << "]\n";
    std::cout << std::right;             // reset

    // ============================================================
    //  3. FILL -- ⚠️ yeh STICKY hai
    // ============================================================
    std::cout << "\n===== 3. setfill (STICKY!) =====\n";
    std::cout << "setfill('0'): [" << std::setfill('0') << std::setw(6) << 42 << "]\n";
    std::cout << "abhi bhi '0': [" << std::setw(6) << 7 << "]  <- sticky hai!\n";
    std::cout << std::setfill(' ');      // ⚠️ RESET karna ZAROORI hai
    std::cout << "reset ke baad: [" << std::setw(6) << 42 << "]\n";

    // ============================================================
    //  4. FLOATING POINT -- setprecision ka mode-dependent behaviour
    // ============================================================
    std::cout << "\n===== 4. FLOATING POINT =====\n";
    const double pi = 3.14159265358979;

    std::cout << "default (6 significant): " << pi << "\n";
    std::cout << "setprecision(3):         " << std::setprecision(3) << pi
              << "   <- 3 SIGNIFICANT digits\n";
    std::cout << "setprecision(10):        " << std::setprecision(10) << pi << "\n";

    std::cout << "fixed + precision(2):    " << std::fixed << std::setprecision(2)
              << pi << "     <- 2 digits AFTER decimal\n";
    std::cout << "fixed + precision(6):    " << std::setprecision(6) << pi << "\n";

    std::cout << "scientific:              " << std::scientific << std::setprecision(3)
              << pi << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);   // reset

    // Mode ka fark saaf dekho
    std::cout << "\n⚠️ setprecision ka matlab MODE pe depend karta hai:\n";
    const double x = 1234.5678;
    std::cout << "  1234.5678, setprecision(3), default mode: ";
    std::cout << std::setprecision(3) << x << "   <- 3 significant digits\n";
    std::cout << "  1234.5678, setprecision(3), fixed mode:   ";
    std::cout << std::fixed << std::setprecision(3) << x << "  <- 3 decimals\n";
    std::cout << std::defaultfloat << std::setprecision(6);

    // ============================================================
    //  5. NUMBER BASE -- ⚠️ STICKY
    // ============================================================
    std::cout << "\n===== 5. NUMBER BASE (STICKY!) =====\n";
    const int n = 255;
    std::cout << "dec: " << std::dec << n << "\n";
    std::cout << "oct: " << std::oct << n << "\n";
    std::cout << "hex: " << std::hex << n << "\n";
    std::cout << "HEX: " << std::uppercase << std::hex << n << "\n";
    std::cout << std::nouppercase;
    std::cout << "0x:  " << std::showbase << std::hex << n << "\n";
    std::cout << std::noshowbase;

    std::cout << "abhi bhi hex: " << 100 << "  <- 64! reset nahi kiya\n";
    std::cout << std::dec;               // ⚠️ RESET
    std::cout << "reset ke baad: " << 100 << "\n";

    // ============================================================
    //  6. BOOL, SIGN
    // ============================================================
    std::cout << "\n===== 6. BOOL / SIGN =====\n";
    std::cout << "default:    " << true << "\n";
    std::cout << "boolalpha:  " << std::boolalpha << true << "\n";
    std::cout << std::noboolalpha;
    std::cout << "showpos:    " << std::showpos << 42 << "\n";
    std::cout << std::noshowpos;

    // ============================================================
    //  7. PRACTICAL: ek table banao
    // ============================================================
    std::cout << "\n===== 7. TABLE =====\n";
    std::cout << std::left  << std::setw(12) << "Symbol"
              << std::right << std::setw(12) << "Price"
              << std::setw(8)  << "Qty"
              << std::setw(14) << "Value" << "\n";
    std::cout << std::string(46, '-') << "\n";

    struct Row { const char* sym; double price; int qty; };
    const Row rows[] = {
        {"NIFTY",     21500.50, 100},
        {"BANKNIFTY", 46200.25,  50},
        {"RELIANCE",   2890.75, 200},
    };

    for (const auto& r : rows) {
        std::cout << std::left  << std::setw(12) << r.sym
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2)
                  << r.price
                  << std::setw(8)  << r.qty
                  << std::setw(14) << (r.price * r.qty) << "\n";
    }
    std::cout << std::defaultfloat << std::setprecision(6);

    // ============================================================
    //  8. MONEY -- integer paise se (folder 03 file 06 ka rule)
    // ============================================================
    std::cout << "\n===== 8. MONEY (integer paise) =====\n";
    // ⚠️ Money ke liye double MAT use karo -- rounding errors aate hain.
    //    Integer paise use karo, aur display ke waqt hi format karo.
    const std::int64_t paise = 2150050;      // Rs 21500.50
    std::cout << "Rs " << (paise / 100) << "."
              << std::setfill('0') << std::setw(2) << (paise % 100)
              << std::setfill(' ') << "\n";
    std::cout << "(exact hai -- koi floating point error nahi)\n";

    // ============================================================
    //  9. HEX DUMP -- market data debugging mein bahut kaam aata hai
    // ============================================================
    std::cout << "\n===== 9. HEX DUMP =====\n";
    {
        IosStateSaver saver(std::cout);      // ✅ state automatically restore hogi

        const unsigned char data[] = {
            0x41, 0x42, 0x43, 0x44, 0x00, 0x27, 0x10, 0xFF,
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
            0x01, 0x02, 0x03
        };

        std::cout << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < sizeof(data); ++i) {
            if (i % 16 == 0) {
                std::cout << std::setw(4) << i << ": ";
            }
            std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
            if (i % 16 == 7) std::cout << " ";
            if (i % 16 == 15) std::cout << "\n";
        }
        std::cout << "\n";
    }   // <- yahan IosStateSaver ne hex/fill wapas normal kar diya

    std::cout << "State restore ho gayi -- yeh decimal mein: " << 255 << "\n";
    std::cout << "(RAII ne kaam kiya!)\n";

    return 0;
}
