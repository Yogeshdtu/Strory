// 05_file_word_stats.cpp  --  49-PROJECTS beginner P5
// ============================================================
// Text statistics: lines / words / chars, top-N words (case-insensitive,
// punctuation stripped), average word length, longest line.
// I/O is via std::istream so a fixture string drives the tests; a real
// main() would open an std::ifstream (RAII) and error on "not found".
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 05_file_word_stats.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct Stats {
    std::size_t lines = 0;
    std::size_t words = 0;
    std::size_t chars = 0;            // excluding newlines
    std::size_t longest_line = 0;
    double      avg_word_len = 0.0;
    std::unordered_map<std::string, int> freq;
};

std::string normalize(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        const auto c = static_cast<unsigned char>(ch);   // cast to unsigned char FIRST
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;                                       // "" if the token was all punctuation
}

Stats analyze(std::istream& in) {
    Stats s;
    std::string line;
    std::size_t total_word_chars = 0;
    while (std::getline(in, line)) {
        ++s.lines;
        s.chars += line.size();
        s.longest_line = std::max(s.longest_line, line.size());
        std::istringstream ls(line);
        std::string tok;
        while (ls >> tok) {
            const std::string w = normalize(tok);
            if (w.empty()) continue;
            ++s.words;
            total_word_chars += w.size();
            ++s.freq[w];
        }
    }
    s.avg_word_len = s.words ? static_cast<double>(total_word_chars) / static_cast<double>(s.words) : 0.0;
    return s;
}

// Top-n by frequency desc, ties broken alphabetically. partial_sort, not full.
std::vector<std::pair<std::string, int>> top_n(const Stats& s, std::size_t n) {
    std::vector<std::pair<std::string, int>> v(s.freq.begin(), s.freq.end());
    if (n > v.size()) n = v.size();
    std::partial_sort(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n), v.end(),
                      [](const auto& a, const auto& b) {
                          if (a.second != b.second) return a.second > b.second;
                          return a.first < b.first;
                      });
    v.resize(n);
    return v;
}

} // namespace

int main() {
    const char* fixture =
        "The quick brown fox.\n"
        "The QUICK dog!\n"
        "\n"
        "the fox, the fox, the FOX?\n";

    std::istringstream in(fixture);
    const Stats s = analyze(in);

    assert(s.lines == 4);
    // words: line1 4, line2 3, line3 0, line4 6  -> 13
    assert(s.words == 13);
    // chars per line (no newline): 20 + 14 + 0 + 26 = 60
    assert(s.chars == 60);
    assert(s.longest_line == 26);

    assert(s.freq.at("the") == 5);       // l1 "The", l2 "The", l4 "the","the","the"
    assert(s.freq.at("fox") == 4);       // l1 "fox.", l4 "fox,","fox,","FOX?"
    assert(s.freq.at("quick") == 2);
    assert(s.freq.at("dog") == 1);
    assert(s.freq.count("brown") == 1 && s.freq.at("brown") == 1);

    const auto top3 = top_n(s, 3);
    assert(top3.size() == 3);
    assert(top3[0].first == "the" && top3[0].second == 5);
    assert(top3[1].first == "fox" && top3[1].second == 4);
    assert(top3[2].first == "quick" && top3[2].second == 2);

    // avg word length: total normalized word chars / 13 words
    // "the"x5=15, "quick"x2=10, "brown"5, "fox"x4=12, "dog"3  => 45 / 13
    assert(std::abs(s.avg_word_len - 45.0 / 13.0) < 1e-9);

    // missing-file behaviour (what a real main would do)
    {
        std::ifstream f("does_not_exist_49p5.txt");
        assert(!f.is_open());            // -> print error, return non-zero
    }

    std::puts("05_file_word_stats: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - normalize(): std::isalnum/std::tolower are UB on values outside
//     unsigned char / EOF -> cast to unsigned char BEFORE calling (11/UB).
//   - I/O via std::istream& -> the same code works on a file or a fixture.
//     The real main opens an std::ifstream (RAII closes it) and checks is_open().
//   - top_n: partial_sort + a tie-break comparator (freq desc, then name asc)
//     for a stable, deterministic result (19).
// ============================================================
