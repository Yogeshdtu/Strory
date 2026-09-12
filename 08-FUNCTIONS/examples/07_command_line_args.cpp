// 07_command_line_args.cpp
// ============================================================
// main(int argc, char* argv[]) -- program ko bahar se input
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 07_command_line_args.cpp -o args
//   ./args hello world 10 20 30
//   ./args --sum 3 4 5
//   ./args
// ============================================================
// argc = argument COUNT (kam se kam 1)
// argv = argument VECTOR -- C-strings ka array
//   argv[0]       = program ka naam/path
//   argv[1..argc-1] = user ke diye arguments
//   argv[argc]    = nullptr  (array yahan khatam)
// ============================================================

#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>

// ek string_view ko int mein parse karo (safe, exceptions ke bina)
static std::optional<long long> toInt(std::string_view s) {
    long long value = 0;
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec == std::errc{} && ptr == last) return value;
    return std::nullopt;
}

int main(int argc, char* argv[]) {
    // ============================================================
    //  1. argc / argv[0]
    // ============================================================
    std::cout << "argc = " << argc << "\n";
    std::cout << "argv[0] (program) = " << argv[0] << "\n";

    // ============================================================
    //  2. Saare arguments print karo
    // ============================================================
    std::cout << "\nSaare arguments:\n";
    for (int i = 0; i < argc; ++i) {
        std::cout << "  argv[" << i << "] = \"" << argv[i] << "\"\n";
    }
    // proof: argv[argc] hamesha nullptr
    std::cout << "  argv[argc] = " << (argv[argc] == nullptr ? "nullptr" : "??")
              << "   (array yahan khatam)\n";

    // ============================================================
    //  3. Flag parse karo:  --sum  ke baad ke numbers jodo
    // ============================================================
    bool sumMode = false;
    long long total = 0;
    int parsedNumbers = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--sum") {
            sumMode = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "\nUsage: " << argv[0] << " [--sum] [numbers...]\n";
            return 0;
        }

        if (sumMode) {
            if (const auto n = toInt(arg)) {
                total += *n;
                ++parsedNumbers;
            } else {
                std::cerr << "  '" << arg << "' number nahi hai -- skip\n";
            }
        }
    }

    if (sumMode) {
        std::cout << "\n--sum: " << parsedNumbers << " numbers ka total = " << total << "\n";
    }

    // ============================================================
    //  4. Kuch bhi arg nahi diya
    // ============================================================
    if (argc == 1) {
        std::cout << "\n(koi argument nahi diya. Try: " << argv[0]
                  << " --sum 10 20 30)\n";
    }

    return 0;
}
