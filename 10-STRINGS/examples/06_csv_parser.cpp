// 06_csv_parser.cpp
// ============================================================
// Chhota CSV parser -- zero-copy (string_view) + from_chars
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 06_csv_parser.cpp -o csv && ./csv
// ============================================================
// Ek chhota "connected project": trade records ka CSV parse karke
// aggregate stats nikaalo. Ki folder concepts ek saath --
//   string_view (no copy), from_chars (fast parse), structs (folder 11 preview),
//   error handling (guard clauses, folder 06).
// ============================================================

#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

// ------------------------------------------------------------
//  ek line ke fields (source string ke andar VIEWS -- koi copy nahi)
// ------------------------------------------------------------
static std::vector<std::string_view> splitFields(std::string_view line, char delim = ',') {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = line.find(delim, start);
        if (pos == std::string_view::npos) { out.push_back(line.substr(start)); break; }
        out.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

static std::optional<long long> toInt(std::string_view s) {
    long long v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && p == s.data() + s.size()) return v;
    return std::nullopt;
}
static std::optional<double> toDouble(std::string_view s) {
    double v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && p == s.data() + s.size()) return v;
    return std::nullopt;
}

// ------------------------------------------------------------
//  parsed record  (folder 11 mein struct deep dive)
// ------------------------------------------------------------
struct Trade {
    std::string_view symbol;      // ⚠️ view -- source CSV zinda rehna chahiye
    double price = 0.0;
    long long qty = 0;
    std::string_view side;
};

static std::optional<Trade> parseTrade(std::string_view line) {
    auto f = splitFields(line);
    if (f.size() != 4) return std::nullopt;               // guard: 4 fields chahiye

    Trade t;
    t.symbol = f[0];
    if (t.symbol.empty()) return std::nullopt;

    if (auto px = toDouble(f[1])) t.price = *px; else return std::nullopt;
    if (auto q  = toInt(f[2]))    t.qty   = *q;  else return std::nullopt;
    if (t.price <= 0.0 || t.qty <= 0) return std::nullopt;

    t.side = f[3];
    if (t.side != "BUY" && t.side != "SELL") return std::nullopt;
    return t;
}

int main() {
    // input CSV -- ek hi contiguous buffer; saare string_view isme point karte hain
    const std::string_view csv =
        "AAPL,192.34,100,BUY\n"
        "MSFT,410.10,250,SELL\n"
        "NVDA,880.00,50,BUY\n"
        "BADROW,not_a_price,10,BUY\n"        // parse fail -- skip
        "TSLA,175.20,0,BUY\n"                // qty 0 -- skip
        "GOOG,152.75,300,SELL\n";

    std::cout << "===== parsing CSV =====\n";
    long long totalShares = 0;
    double buyNotional = 0.0, sellNotional = 0.0;
    int ok = 0, bad = 0, lineNo = 0;

    std::size_t start = 0;
    while (start < csv.size()) {
        ++lineNo;
        std::size_t nl = csv.find('\n', start);
        std::string_view line = csv.substr(start, nl - start);
        start = (nl == std::string_view::npos) ? csv.size() : nl + 1;
        if (line.empty()) continue;

        if (auto tr = parseTrade(line)) {
            ++ok;
            totalShares += tr->qty;
            const double notional = tr->price * static_cast<double>(tr->qty);
            if (tr->side == "BUY") buyNotional += notional; else sellNotional += notional;
            std::cout << "  line " << lineNo << ": " << tr->symbol << "  "
                      << tr->qty << " @ " << tr->price << "  " << tr->side << "\n";
        } else {
            ++bad;
            std::cout << "  line " << lineNo << ": SKIP  (\"" << line << "\")\n";
        }
    }

    std::cout << "\n===== summary =====\n";
    std::cout << "  parsed OK       : " << ok << "\n";
    std::cout << "  skipped (bad)   : " << bad << "\n";
    std::cout << "  total shares    : " << totalShares << "\n";
    std::cout << "  buy notional    : " << buyNotional << "\n";
    std::cout << "  sell notional   : " << sellNotional << "\n";
    std::cout << "  net (buy - sell): " << (buyNotional - sellNotional) << "\n";

    std::cout <<
        "\n  Zero-copy: har field ek view hai `csv` ke andar -- 0 std::string banaye.\n"
        "  ⚠️ Trade.symbol/side views hain -- `csv` in views se zyada zinda rehna chahiye.\n"
        "  Agar records ko store/return karna ho -> owning std::string, ya csv ko\n"
        "  poore record set ke lifetime tak zinda rakho. (Lesson 09.)\n";

    return 0;
}
