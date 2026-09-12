// 05_unions.cpp
// ============================================================
// union -- ek hi memory ki types ke liye share
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_unions.cpp -o un && ./un
// ============================================================
// union ke saare members SAME memory pe rehte hain. Size = sabse bade
// member jitna. Ek waqt mein ek hi member "active" hota hai.
// Galat member padhna -> UB (type punning) -- par kuch cases well-defined.
// ============================================================

#include <cstdint>
#include <cstring>
#include <iostream>

union Value {
    std::int32_t i;
    float        f;
    char         bytes[4];
};   // sizeof(Value) == 4  (sabse bada member)

// tagged union -- "active member" ko track karna (yeh std::variant ka manual version)
struct TaggedValue {
    enum class Tag { Int, Float, Text } tag;
    union {
        std::int64_t i;
        double       d;
        char         text[8];
    };
};

int main() {
    // ============================================================
    //  1. Members SAME memory share karte hain
    // ============================================================
    std::cout << "===== 1. shared memory =====\n";
    std::cout << "  sizeof(Value) = " << sizeof(Value) << "  (max of 4, 4, 4)\n";

    Value v;
    v.i = 0x41424344;                 // 'A'=0x41 'B'=0x42 'C'=0x43 'D'=0x44
    std::cout << "  v.i = 0x" << std::hex << v.i << std::dec << "\n";
    std::cout << "  v.bytes = ";
    for (char c : v.bytes) std::cout << "'" << c << "' ";
    std::cout << "  <- SAME bytes, char[] ke through padhe (little-endian order)\n";

    // ============================================================
    //  2. ⚠️ Type punning -- "galat" member padhna
    // ============================================================
    std::cout << "\n===== 2. type punning =====\n";
    v.f = 1.0f;
    std::cout << "  v.f = 1.0f  ->  v.i (as bits) = 0x" << std::hex << v.i << std::dec
              << "  (IEEE-754 of 1.0f)\n";
    std::cout << "  ⚠️ C++ mein: last-written member ke alawa padhna technically UB.\n"
                 "     (C mein allowed; GCC/Clang practically allow karte hain.)\n";

    // ✅ Sahi type punning: std::bit_cast (C++20) ya memcpy
    const float src = 3.14f;
    std::uint32_t bits;
    std::memcpy(&bits, &src, sizeof(bits));       // well-defined
    std::cout << "  memcpy(3.14f) bits = 0x" << std::hex << bits << std::dec << "  ✅\n";
    // C++20: std::uint32_t bits = std::bit_cast<std::uint32_t>(src);

    // ============================================================
    //  3. Tagged union -- active member track karo
    // ============================================================
    std::cout << "\n===== 3. tagged union =====\n";
    TaggedValue tv;
    tv.tag = TaggedValue::Tag::Float;
    tv.d = 2.71828;

    auto printTagged = [](const TaggedValue& t) {
        switch (t.tag) {
            case TaggedValue::Tag::Int:   std::cout << "  int:   " << t.i << "\n"; break;
            case TaggedValue::Tag::Float: std::cout << "  float: " << t.d << "\n"; break;
            case TaggedValue::Tag::Text:  std::cout << "  text:  " << t.text << "\n"; break;
        }
    };
    printTagged(tv);
    tv.tag = TaggedValue::Tag::Int;   tv.i = 42;
    printTagged(tv);

    std::cout << "  sizeof(TaggedValue) = " << sizeof(TaggedValue)
              << "  (tag + 8-byte union + padding)\n";

    // ============================================================
    //  4. Kab union use karo -- aur kab NAHI
    // ============================================================
    std::cout <<
        "\n===== 4. guidance =====\n"
        "  ✅ union theek: fixed wire protocols (message ka ek variant), memory-tight\n"
        "     embedded, low-level bit views (with memcpy/bit_cast).\n"
        "  ❌ union avoid: general 'variant' type -> std::variant use karo (06_variant.cpp)\n"
        "     -- woh active member track karta hai aur non-trivial types (std::string)\n"
        "     ko safely handle karta hai. Raw union mein woh manual + error-prone hai.\n"
        "  ⚠️ Non-trivial member (std::string) wala union -> constructor/destructor\n"
        "     KHUD likhna padta hai (placement new / explicit ~). Bahut galti-prone.\n";

    return 0;
}
