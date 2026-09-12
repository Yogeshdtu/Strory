// 07_stringstream_parse.cpp
// ============================================================
// String streams -- parsing, building, aur unke fast alternatives
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_stringstream_parse.cpp -o ssparse
//   ./ssparse
// ============================================================

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <optional>
#include <chrono>
#include <iomanip>
#include <cstdint>

// ============================================================
//  Tareeka 1: istringstream se split -- SIMPLE par SLOW
// ============================================================
std::vector<std::string> splitStream(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::istringstream iss(line);          // ⚠️ har call pe object banta hai
    std::string part;
    while (std::getline(iss, part, delim)) {
        parts.push_back(part);             // ⚠️ har part pe string COPY + alloc
    }
    return parts;
}

// ============================================================
//  Tareeka 2: string_view se split -- FAST (koi copy nahi)
// ============================================================
// string_view sirf ek POINTER + LENGTH hai. Koi memory allocate nahi hoti.
// ⚠️ Views original string ko point karte hain -- agar woh mar gayi,
//    views DANGLING ho jaayenge. (Folder 10 mein detail.)
std::vector<std::string_view> splitView(std::string_view s, char delim) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

// ============================================================
//  Safe integer parsing -- from_chars (C++17)
// ============================================================
// Faayde:
//   - koi allocation nahi
//   - koi exception nahi
//   - trailing garbage DETECT karta hai ("12abc" reject)
//   - sabse tez
std::optional<std::int64_t> parseInt(std::string_view s) {
    std::int64_t value{};
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{}) return std::nullopt;      // parse fail / out of range
    if (ptr != last)       return std::nullopt;      // trailing garbage
    return value;
}

// ============================================================
//  Trade record
// ============================================================
struct Trade {
    std::string   symbol;
    std::int64_t  priceInTicks;
    std::uint32_t quantity;
    char          side;
};

bool parseTrade(std::string_view line, Trade& t) {
    const auto fields = splitView(line, ',');
    if (fields.size() != 4) return false;

    t.symbol = std::string(fields[0]);

    const auto price = parseInt(fields[1]);
    if (!price) return false;
    t.priceInTicks = *price;

    const auto qty = parseInt(fields[2]);
    if (!qty || *qty < 0) return false;
    t.quantity = static_cast<std::uint32_t>(*qty);

    if (fields[3].size() != 1) return false;
    t.side = fields[3][0];

    return true;
}

int main() {
    // ============================================================
    //  1. STRING BUILDING
    // ============================================================
    std::cout << "===== 1. STRING BUILDING =====\n";
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Order: " << "NIFTY" << " @ " << 21500.50 << " x " << 100;
        const std::string result = oss.str();
        std::cout << "  " << result << "\n";
        std::cout << "  (C++20 mein std::format behtar hai -- file 08 dekho)\n";
    }

    // ============================================================
    //  2. PARSING
    // ============================================================
    std::cout << "\n===== 2. BASIC PARSING =====\n";
    {
        std::istringstream iss("42 3.14 hello");
        int i = 0;
        double d = 0;
        std::string s;
        iss >> i >> d >> s;
        std::cout << "  int=" << i << " double=" << d << " string=" << s << "\n";
    }

    // ============================================================
    //  3. CSV PARSING -- dono tareeke
    // ============================================================
    std::cout << "\n===== 3. CSV PARSING =====\n";
    const std::string csvLine = "NIFTY,2150050,100,B";

    {
        const auto parts = splitStream(csvLine, ',');
        std::cout << "  istringstream se (" << parts.size() << " fields): ";
        for (const auto& p : parts) std::cout << "[" << p << "] ";
        std::cout << "\n";
    }
    {
        const auto parts = splitView(csvLine, ',');
        std::cout << "  string_view se  (" << parts.size() << " fields): ";
        for (const auto& p : parts) std::cout << "[" << p << "] ";
        std::cout << "\n";
    }

    // ============================================================
    //  4. TRADE PARSING
    // ============================================================
    std::cout << "\n===== 4. TRADE PARSING =====\n";
    const char* lines[] = {
        "NIFTY,2150050,100,B",
        "BANKNIFTY,4620025,50,S",
        "BAD,notanumber,10,B",           // ⚠️ galat
        "INCOMPLETE,123",                // ⚠️ kam fields
        "TRAILING,12abc,10,B",           // ⚠️ trailing garbage
    };

    for (const char* line : lines) {
        Trade t{};
        if (parseTrade(line, t)) {
            std::cout << "  ✅ " << std::setw(12) << std::left << t.symbol
                      << " Rs " << (t.priceInTicks / 100) << "."
                      << std::setfill('0') << std::setw(2) << (t.priceInTicks % 100)
                      << std::setfill(' ')
                      << " x " << t.quantity << " " << t.side << "\n";
        } else {
            std::cout << "  ❌ Parse fail: " << line << "\n";
        }
    }
    std::cout << std::right;

    // ============================================================
    //  5. ⚠️ REUSE TRAP
    // ============================================================
    std::cout << "\n===== 5. REUSE TRAP =====\n";
    {
        std::stringstream ss;
        ss << "42";
        int a = 0;
        ss >> a;                       // ab eofbit SET ho gaya

        // ❌ Sirf str() -- flags saaf nahi hue
        ss.str("100");
        int b = -1;
        ss >> b;
        std::cout << "  Sirf str(\"100\"): a=" << a << " b=" << b
                  << "   <- b nahi badla (stream fail state mein hai)\n";

        // ✅ str() + clear()
        ss.str("100");
        ss.clear();                    // ✅ flags saaf
        int c = -1;
        ss >> c;
        std::cout << "  str() + clear(): c=" << c << "   <- ab kaam kiya\n";
    }

    // ============================================================
    //  6. ⚠️ stoi KA TRAILING GARBAGE TRAP
    // ============================================================
    std::cout << "\n===== 6. stoi vs from_chars =====\n";
    {
        const std::string bad = "12abc";
        std::cout << "  Input: \"" << bad << "\"\n";
        std::cout << "    std::stoi:  " << std::stoi(bad)
                  << "        <- ⚠️ accept kar liya! garbage ignore\n";
        const auto safe = parseInt(bad);
        std::cout << "    from_chars: "
                  << (safe ? std::to_string(*safe) : std::string("REJECTED"))
                  << "  <- ✅ sahi se reject kiya\n";
    }

    // ============================================================
    //  7. BENCHMARK
    // ============================================================
    std::cout << "\n===== 7. BENCHMARK =====\n";
    const int N = 100000;
    const std::string testLine = "NIFTY,2150050,100,B";

    auto t1 = std::chrono::steady_clock::now();
    std::size_t sink1 = 0;
    for (int i = 0; i < N; ++i) {
        const auto parts = splitStream(testLine, ',');
        sink1 += parts.size();
    }
    auto t2 = std::chrono::steady_clock::now();

    std::size_t sink2 = 0;
    for (int i = 0; i < N; ++i) {
        const auto parts = splitView(testLine, ',');
        sink2 += parts.size();
    }
    auto t3 = std::chrono::steady_clock::now();

    // Number conversion benchmark
    const std::string numStr = "2150050";
    std::int64_t sink3 = 0;
    auto t4 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        std::istringstream iss(numStr);
        std::int64_t v = 0;
        iss >> v;
        sink3 += v;
    }
    auto t5 = std::chrono::steady_clock::now();

    std::int64_t sink4 = 0;
    for (int i = 0; i < N; ++i) {
        sink4 += std::stoll(numStr);
    }
    auto t6 = std::chrono::steady_clock::now();

    std::int64_t sink5 = 0;
    for (int i = 0; i < N; ++i) {
        std::int64_t v = 0;
        std::from_chars(numStr.data(), numStr.data() + numStr.size(), v);
        sink5 += v;
    }
    auto t7 = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  SPLITTING (" << N << " lines):\n";
    std::cout << "    istringstream: " << std::setw(8) << ms(t1, t2) << " ms\n";
    std::cout << "    string_view:   " << std::setw(8) << ms(t2, t3) << " ms"
              << "   <- " << (ms(t1,t2) / ms(t2,t3)) << "x faster\n";

    std::cout << "\n  NUMBER PARSING (" << N << " conversions):\n";
    std::cout << "    istringstream: " << std::setw(8) << ms(t4, t5) << " ms\n";
    std::cout << "    std::stoll:    " << std::setw(8) << ms(t5, t6) << " ms"
              << "   <- " << (ms(t4,t5) / ms(t5,t6)) << "x faster\n";
    std::cout << "    from_chars:    " << std::setw(8) << ms(t6, t7) << " ms"
              << "   <- " << (ms(t4,t5) / ms(t6,t7)) << "x faster\n";

    // Sinks ko use karo taaki compiler optimize na kar de
    if (sink1 + sink2 == 0 || sink3 + sink4 + sink5 == 0) std::cout << "";

    std::cout << "\n  HFT RELEVANCE:\n";
    std::cout << "    istringstream har call pe object banata hai aur allocate karta hai.\n";
    std::cout << "    Hot path mein bilkul nahi.\n";
    std::cout << "    Market data parsing mein from_chars + string_view use hote hain --\n";
    std::cout << "    zero allocation, zero copy. Folder 38 mein poora.\n";

    return 0;
}
