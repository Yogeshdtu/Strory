// 07_enums.cpp
// ============================================================
// enum vs enum class -- naam wale constants, aur type safety
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_enums.cpp -o en && ./en
// ============================================================

#include <cstdint>
#include <iostream>
#include <string_view>

// ---- plain (unscoped) enum -- PURANA tareeqa, bacho ----
enum Color { Red, Green, Blue };            // Red=0, Green=1, Blue=2
enum Status { Active, Inactive };           // ⚠️ Active=0 -- Red bhi 0, dono ek doosre se compare ho jaate hain

// ---- enum class (scoped) -- YAHI USE KARO ----
enum class Side : std::uint8_t {            // underlying type khud bataya (1 byte)
    Buy  = 1,
    Sell = 2,
};

enum class OrderType {                      // values apne aap: Market=0, Limit=1, ...
    Market, Limit, Stop, StopLimit,
};

// enum class -> string (switch mein default NAHI -> case chhoota to -Wswitch pakdega)
std::string_view toString(OrderType t) {
    switch (t) {
        case OrderType::Market:    return "Market";
        case OrderType::Limit:     return "Limit";
        case OrderType::Stop:      return "Stop";
        case OrderType::StopLimit: return "StopLimit";
    }
    return "?";
}

int main() {
    // ============================================================
    //  1. Plain enum -- implicit int conversion (aur uske traps)
    // ============================================================
    std::cout << "===== 1. plain enum =====\n";
    Color c = Green;
    int ci = c;                              // ⚠️ chupchaap -> int  (kabhi kaam ka, kabhi bug)
    std::cout << "  Green as int = " << ci << "\n";
    std::cout << "  ⚠️ Red aur Active dono 0 -- `Red == Active` compiles (-Wenum-compare)\n"
                 "     aur logic bug ban sakta hai. Scope nahi hai unscoped enum mein.\n";
    // enum Color x = 5;                      // ❌ C++ mein ERROR: invalid conversion from 'int' to 'Color'
    //                                        //    (ulti disha -- int -> enum -- chupchaap nahi hoti; sirf -fpermissive se warning)

    // ============================================================
    //  2. enum class -- scoped, no implicit conversion
    // ============================================================
    std::cout << "\n===== 2. enum class =====\n";
    Side s = Side::Buy;
    // int si = s;                            // ❌ ERROR -- chupchaap conversion nahi (ACHHA hai)
    int si = static_cast<int>(s);             // sirf khul ke cast
    std::cout << "  Side::Buy = " << si << "  (underlying uint8_t)\n";
    std::cout << "  sizeof(Side) = " << sizeof(Side) << " byte (: uint8_t se)\n";

    // s == 1;                                // ❌ ERROR -- raw int se compare nahi kar sakte
    if (s == Side::Buy) std::cout << "  s == Side::Buy  ✅ (type-safe compare)\n";

    // ============================================================
    //  3. enum class + switch -- exhaustiveness
    // ============================================================
    std::cout << "\n===== 3. switch dispatch =====\n";
    for (OrderType t : {OrderType::Market, OrderType::Limit, OrderType::Stop, OrderType::StopLimit})
        std::cout << "  " << static_cast<int>(t) << " -> " << toString(t) << "\n";
    std::cout << "  (toString mein NO default -> naya OrderType add karne pe -Wswitch warn karega)\n";

    // ============================================================
    //  4. Flags with enum class (bitwise operators define karne padte hain)
    // ============================================================
    std::cout << "\n===== 4. flags =====\n";
    enum class Flag : std::uint32_t {
        None     = 0,
        Hidden   = 1u << 0,
        PostOnly = 1u << 1,
        IOC      = 1u << 2,
    };
    auto operator_or = [](Flag a, Flag b) {
        return static_cast<Flag>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    };
    auto has = [](Flag set, Flag f) {
        return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(f)) != 0;
    };
    Flag f = operator_or(Flag::Hidden, Flag::IOC);
    std::cout << "  flags = Hidden | IOC\n";
    std::cout << "  has(Hidden)?   " << std::boolalpha << has(f, Flag::Hidden) << "\n";
    std::cout << "  has(PostOnly)? " << has(f, Flag::PostOnly) << "\n";
    std::cout << "  (real code mein operator| / operator& overload karo -- folder 05 file 05)\n";

    // ============================================================
    //  5. underlying type + std::to_underlying (C++23)
    // ============================================================
    std::cout << "\n===== 5. underlying type =====\n";
    std::cout << "  OrderType underlying default = int, sizeof = " << sizeof(OrderType) << "\n";
    std::cout << "  Side underlying = uint8_t (explicit :) -> compact structs\n";

    std::cout <<
        "\n"
        "  RULE: enum class hamesha (scoped, type-safe, no surprise int conversion).\n"
        "  underlying type explicit do jab struct mein pack karna ho (: std::uint8_t).\n"
        "  Plain enum sirf legacy / bitmask-with-ADL-ops / interop.\n";

    return 0;
}
