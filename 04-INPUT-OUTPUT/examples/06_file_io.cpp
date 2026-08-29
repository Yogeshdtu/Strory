// 06_file_io.cpp
// ============================================================
// File streams -- text, binary, RAII, error handling
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 06_file_io.cpp -o fileio && ./fileio
//
// Baad mein dekho:
//   cat output.txt
//   cat appended.txt
//   xxd records.bin | head
// ============================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <type_traits>
#include <filesystem>

// ============================================================
//  Binary record -- fixed-width types (folder 03 file 09 ka rule)
// ============================================================
struct Record {
    std::uint64_t id;
    std::int64_t  priceInTicks;
    std::uint32_t quantity;
    std::uint8_t  side;          // 'B' ya 'S'
};

// Binary I/O se pehle COMPILE TIME pe verify karo:
static_assert(std::is_trivially_copyable_v<Record>,
              "Record trivially copyable hona chahiye binary I/O ke liye");

int main() {
    // ============================================================
    //  1. LIKHNA -- RAII ke saath
    // ============================================================
    std::cout << "===== 1. TEXT LIKHNA =====\n";
    {
        std::ofstream out("output.txt");

        // ✅ HAMESHA check karo
        if (!out) {
            std::cerr << "output.txt nahi khul payi\n";
            return 1;
        }

        out << "Line 1\n";
        out << "Number: " << 42 << "\n";
        out << "Price: " << std::fixed << std::setprecision(2) << 21500.5 << "\n";

        // ⚠️ Yahan `endl` MAT use karo -- har line pe flush = slow
        // out << "line" << std::endl;   // ❌

        // out.close();   // ZARURAT NAHI -- destructor kar dega
        std::cout << "output.txt likh di\n";
    }   // <- yahan destructor chala: file BAND hui + buffer FLUSH hua

    // ============================================================
    //  2. PADHNA -- line by line
    // ============================================================
    std::cout << "\n===== 2. LINE BY LINE PADHNA =====\n";
    {
        std::ifstream in("output.txt");
        if (!in) {
            std::cerr << "output.txt nahi mili\n";
            return 1;
        }

        std::string line;
        line.reserve(256);              // ✅ pehle se allocate -- reuse hogi
        int lineNum = 0;

        while (std::getline(in, line)) {
            std::cout << "  [" << ++lineNum << "] " << line << "\n";
        }

        // ⚠️ Loop KYUN khatam hua? Yeh check zaroori hai.
        if (in.eof()) {
            std::cout << "  (file poori padh li -- normal)\n";
        } else if (in.bad()) {
            std::cerr << "  (I/O error!)\n";
        } else if (in.fail()) {
            std::cerr << "  (parse error!)\n";
        }
    }

    // ============================================================
    //  3. ⚠️ TRUNCATE vs APPEND
    // ============================================================
    std::cout << "\n===== 3. TRUNCATE vs APPEND =====\n";

    // ofstream DEFAULT se file ko TRUNCATE karta hai -- poora content mit jaata hai!
    { std::ofstream out("appended.txt"); out << "Pehli baar\n"; }
    { std::ofstream out("appended.txt"); out << "Doosri baar\n"; }
    // ^ "Pehli baar" GAYAB ho gaya!

    {
        std::ifstream in("appended.txt");
        std::string line;
        std::cout << "  Bina app mode ke: ";
        while (std::getline(in, line)) std::cout << "[" << line << "] ";
        std::cout << "  <- pehli line gayab!\n";
    }

    // ✅ Append mode
    { std::ofstream out("appended.txt"); out << "Line A\n"; }          // truncate
    { std::ofstream out("appended.txt", std::ios::app); out << "Line B\n"; }
    { std::ofstream out("appended.txt", std::ios::app); out << "Line C\n"; }

    {
        std::ifstream in("appended.txt");
        std::string line;
        std::cout << "  app mode ke saath:  ";
        while (std::getline(in, line)) std::cout << "[" << line << "] ";
        std::cout << "  <- sab bache\n";
    }

    // ============================================================
    //  4. BINARY I/O
    // ============================================================
    std::cout << "\n===== 4. BINARY I/O =====\n";

    const std::vector<Record> records = {
        {1001, 2150050, 100, 'B'},
        {1002, 2150075,  50, 'S'},
        {1003, 2149900, 200, 'B'},
    };

    // LIKHNA
    {
        // ⚠️ `std::ios::binary` HAMESHA likho binary data ke liye.
        //    Windows pe text mode '\n' ko '\r\n' mein badal deta hai -> corruption.
        std::ofstream out("records.bin", std::ios::binary);
        if (!out) { std::cerr << "records.bin nahi khuli\n"; return 1; }

        for (const auto& r : records) {
            // reinterpret_cast yahan zaroori hai -- write() char* leta hai.
            // Yeh legal hai kyunki hum object ke BYTES padh rahe hain.
            out.write(reinterpret_cast<const char*>(&r), sizeof(r));
        }
        std::cout << "  " << records.size() << " records likhe ("
                  << records.size() * sizeof(Record) << " bytes)\n";
    }

    // PADHNA
    {
        std::ifstream in("records.bin", std::ios::binary);
        if (!in) { std::cerr << "records.bin nahi mili\n"; return 1; }

        Record r{};
        int count = 0;
        while (in.read(reinterpret_cast<char*>(&r), sizeof(r))) {
            std::cout << "  Record " << ++count << ": id=" << r.id
                      << " price=Rs " << (r.priceInTicks / 100) << "."
                      << std::setfill('0') << std::setw(2) << (r.priceInTicks % 100)
                      << std::setfill(' ')
                      << " qty=" << r.quantity
                      << " side=" << static_cast<char>(r.side) << "\n";
        }

        // ⚠️ Partial read check
        if (in.gcount() != 0 && in.gcount() != sizeof(Record)) {
            std::cerr << "  ⚠️ Adhoora record mila! (" << in.gcount() << " bytes)\n";
        }
    }

    std::cout << "\n  sizeof(Record) = " << sizeof(Record) << " bytes\n";
    std::cout << "  ⚠️ Binary I/O ke khatre:\n";
    std::cout << "     - padding (compiler pe depend karti hai)\n";
    std::cout << "     - endianness (alag machine pe bytes ulte)\n";
    std::cout << "     - type sizes (isliye int64_t use kiya, long nahi)\n";
    std::cout << "     - pointers/virtual functions KABHI serialize mat karo\n";

    // ============================================================
    //  5. FILE SIZE
    // ============================================================
    std::cout << "\n===== 5. FILE SIZE =====\n";
    {
        // Tareeka 1: seekg + tellg
        std::ifstream in("records.bin", std::ios::binary);
        in.seekg(0, std::ios::end);
        const auto sizeViaSeek = in.tellg();
        in.seekg(0, std::ios::beg);
        std::cout << "  seekg/tellg:            " << sizeViaSeek << " bytes\n";

        // Tareeka 2: std::filesystem (C++17) -- BEHTAR
        const auto sizeViaFs = std::filesystem::file_size("records.bin");
        std::cout << "  filesystem::file_size:  " << sizeViaFs << " bytes\n";
    }

    // ============================================================
    //  6. POORI FILE EK STRING MEIN
    // ============================================================
    std::cout << "\n===== 6. POORI FILE EK BAAR MEIN =====\n";
    {
        std::ifstream in("output.txt", std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();               // ek hi bada read
        const std::string content = ss.str();
        std::cout << "  " << content.size() << " bytes padhe\n";
        std::cout << "  Pehli line: "
                  << content.substr(0, content.find('\n')) << "\n";
    }

    // ============================================================
    //  7. RAII -- EXCEPTION KE SAATH BHI KAAM KARTA HAI
    // ============================================================
    std::cout << "\n===== 7. RAII + EXCEPTIONS =====\n";
    try {
        std::ofstream out("raii_test.txt");
        out << "Yeh line likh di gayi\n";
        throw std::runtime_error("jaan-boojh kar exception");
        // out << "Yeh kabhi nahi chalegi\n";
    } catch (const std::exception& e) {
        std::cout << "  Exception pakda: " << e.what() << "\n";
    }
    // <- exception ke waqt STACK UNWINDING hui, aur ofstream ka
    //    destructor CHALA -> file band hui, buffer flush hua

    {
        std::ifstream in("raii_test.txt");
        std::string line;
        std::getline(in, line);
        std::cout << "  File mein hai: [" << line << "]\n";
        std::cout << "  ✅ Exception ke bawajood file BAND aur FLUSH hui\n";
        std::cout << "  YEH RAII KA POORA POINT HAI (folder 17 mein detail)\n";
    }

    // ============================================================
    //  8. ERROR HANDLING
    // ============================================================
    std::cout << "\n===== 8. ERROR HANDLING =====\n";
    {
        std::ifstream in("yeh_file_exist_nahi_karti_12345.txt");
        std::cout << "  Missing file kholne pe:\n";
        std::cout << "    (bool)in     = " << static_cast<bool>(in) << "\n";
        std::cout << "    in.is_open() = " << in.is_open() << "\n";
        std::cout << "    in.fail()    = " << in.fail() << "\n";
        std::cout << "  ✅ Isliye HAMESHA `if (!in)` check karo\n";
    }

    std::cout << "\n===== BANI HUI FILES =====\n";
    for (const char* f : {"output.txt", "appended.txt", "records.bin", "raii_test.txt"}) {
        if (std::filesystem::exists(f)) {
            std::cout << "  " << std::setw(16) << std::left << f
                      << std::filesystem::file_size(f) << " bytes\n";
        }
    }
    std::cout << std::right;

    return 0;
}
