// 06_error_code.cpp
// ============================================================
// std::error_code — ek int + ek category pointer (throw-free, allocation-
// free error reporting). errno se, aur ek CUSTOM category se.
// error_code (kya hua, exact) vs error_condition (portable bucket).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 06_error_code.cpp -o ecode && ./ecode
// ============================================================

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>

// ============================================================
//  1. Apna error enum + category
// ============================================================
enum class OrderErr {
    ok = 0,
    unknown_symbol,
    price_out_of_band,
    size_too_large,
    market_closed,
    duplicate_id,
};

// Category = enum ke int codes ko human message + portable condition se jodta hai.
class OrderErrCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "order"; }

    std::string message(int c) const override {
        switch (static_cast<OrderErr>(c)) {
            case OrderErr::ok:                return "ok";
            case OrderErr::unknown_symbol:   return "unknown symbol";
            case OrderErr::price_out_of_band:return "price outside allowed band";
            case OrderErr::size_too_large:   return "size exceeds limit";
            case OrderErr::market_closed:    return "market is closed";
            case OrderErr::duplicate_id:     return "duplicate order id";
        }
        return "unknown order error (" + std::to_string(c) + ")";
    }

    // Optional: apne codes ko standard error_condition buckets se map karo,
    // taaki portable code `ec == std::errc::invalid_argument` likh sake.
    std::error_condition default_error_condition(int c) const noexcept override {
        switch (static_cast<OrderErr>(c)) {
            case OrderErr::unknown_symbol:
            case OrderErr::price_out_of_band:
            case OrderErr::size_too_large:
                return std::errc::invalid_argument;
            case OrderErr::market_closed:
                return std::errc::operation_not_permitted;
            default:
                return std::error_condition(c, *this);
        }
    }
};

// Ek hi global instance (address hi identity hai).
static const OrderErrCategory& order_category() {
    static const OrderErrCategory cat;
    return cat;
}

// enum -> error_code banane ka hook. Isse `std::error_code ec = OrderErr::x;` chalta hai.
static std::error_code make_error_code(OrderErr e) {
    return { static_cast<int>(e), order_category() };
}

// STL ko batao ki OrderErr ek error-code enum hai (implicit conversion allow).
namespace std {
    template <> struct is_error_code_enum<::OrderErr> : true_type {};
}

// ============================================================
//  2. Ek API jo error_code se report karti hai (out-param style)
// ============================================================
struct Order { std::string sym; long price; long qty; };

static long submit(const Order& o, std::error_code& ec) {
    ec.clear();
    if (o.sym != "ACME") { ec = OrderErr::unknown_symbol; return 0; }
    if (o.price <= 0 || o.price > 100000) { ec = OrderErr::price_out_of_band; return 0; }
    if (o.qty > 10000) { ec = OrderErr::size_too_large; return 0; }
    return 900000001;   // assigned order id
}

int main() {
    // --------------------------------------------------------
    //  A. errno -> error_code (system_category / generic_category)
    // --------------------------------------------------------
    std::puts("A. errno se error_code:");
    errno = 0;
    FILE* f = std::fopen("does-not-exist-12345.xyz", "rb");
    if (!f) {
        std::error_code ec(errno, std::generic_category());
        std::printf("   fopen fail: value=%d category=%s message=\"%s\"\n",
                    ec.value(), ec.category().name(), ec.message().c_str());
        // portable check — errno ke number yaad rakhne ki zaroorat nahi:
        if (ec == std::errc::no_such_file_or_directory)
            std::puts("   -> ec == std::errc::no_such_file_or_directory  (portable)");
    } else {
        std::fclose(f);
    }

    // --------------------------------------------------------
    //  B. custom category
    // --------------------------------------------------------
    std::puts("\nB. custom 'order' category:");
    const Order tests[] = {
        {"ACME", 500,   100},
        {"ZZZZ", 500,   100},
        {"ACME", -5,    100},
        {"ACME", 500, 99999},
    };
    for (const auto& o : tests) {
        std::error_code ec;
        long id = submit(o, ec);
        if (ec) {
            std::printf("   %-4s p=%-6ld q=%-6ld -> ERR [%s:%d] %s\n",
                        o.sym.c_str(), o.price, o.qty,
                        ec.category().name(), ec.value(), ec.message().c_str());
            // custom -> standard condition mapping
            if (ec == std::errc::invalid_argument)
                std::puts("        (default_error_condition -> std::errc::invalid_argument)");
        } else {
            std::printf("   %-4s p=%-6ld q=%-6ld -> OK id=%ld\n",
                        o.sym.c_str(), o.price, o.qty, id);
        }
    }

    // --------------------------------------------------------
    //  C. error_code vs error_condition
    // --------------------------------------------------------
    std::puts("\nC. error_code vs error_condition:");
    std::error_code ec = OrderErr::price_out_of_band;
    std::error_condition cond = ec.default_error_condition();
    std::printf("   code : %s:%d \"%s\"   (EXACT: kya-hua)\n",
                ec.category().name(), ec.value(), ec.message().c_str());
    std::printf("   cond : %s:%d \"%s\"   (BUCKET: portable check ke liye)\n",
                cond.category().name(), cond.value(), cond.message().c_str());

    std::puts("\nSaar: error_code = { int value, category* }. Copy = do words.");
    std::puts("Koi throw, koi heap. errno ka type-safe, extensible successor.");
    std::puts("Standard bhi isse use karta: <filesystem>, <thread>, networking TS.");
    return 0;
}
