// 06_config_parser.cpp  --  49-PROJECTS beginner P6
// ============================================================
// INI-ish config parser: [section] headers, key = value, # comments,
// whitespace-tolerant. Typed getters with defaults. Parse errors carry
// the line number. Round-trips (parse -> serialize -> parse).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 06_config_parser.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <charconv>
#include <cstdio>
#include <istream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string_view trim(std::string_view s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string_view::npos) return {};
    const auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

struct ParseError { int line; std::string msg; };

class Config {
public:
    // Returns nullopt on success, or the first error.
    std::optional<ParseError> load(std::istream& in) {
        data_.clear();
        std::string section;
        std::string raw;
        int lineno = 0;
        while (std::getline(in, raw)) {
            ++lineno;
            std::string_view line = trim(raw);
            if (line.empty() || line.front() == '#' || line.front() == ';') continue;

            if (line.front() == '[') {
                if (line.back() != ']')
                    return ParseError{lineno, "unterminated section header"};
                section = std::string(trim(line.substr(1, line.size() - 2)));
                if (section.empty()) return ParseError{lineno, "empty section name"};
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string_view::npos)
                return ParseError{lineno, "expected '=' in 'key = value'"};
            const std::string_view key = trim(line.substr(0, eq));
            const std::string_view val = trim(line.substr(eq + 1));
            if (key.empty()) return ParseError{lineno, "empty key"};

            const std::string full = section.empty() ? std::string(key)
                                                     : section + "." + std::string(key);
            data_[full] = std::string(val);
        }
        return std::nullopt;
    }

    std::string get_string(const std::string& key, std::string def = {}) const {
        const auto it = data_.find(key);
        return it != data_.end() ? it->second : def;
    }
    long long get_int(const std::string& key, long long def = 0) const {
        const auto it = data_.find(key);
        if (it == data_.end()) return def;
        long long out = 0;
        const auto& s = it->second;
        const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        if (ec != std::errc{} || p != s.data() + s.size()) return def;   // not a clean int
        return out;
    }
    bool get_bool(const std::string& key, bool def = false) const {
        const auto it = data_.find(key);
        if (it == data_.end()) return def;
        const std::string& v = it->second;
        if (v == "true" || v == "1" || v == "yes" || v == "on")  return true;
        if (v == "false" || v == "0" || v == "no" || v == "off") return false;
        return def;
    }

    std::string serialize() const {
        std::string out;
        for (const auto& [k, v] : data_) out += k + " = " + v + "\n";
        return out;
    }
    std::size_t size() const { return data_.size(); }

private:
    std::map<std::string, std::string> data_;
};

} // namespace

int main() {
    const char* cfg =
        "# a comment\n"
        "\n"
        "name = CPP Mastery\n"
        "  spaced_key   =   spaced value  \n"
        "[db]\n"
        "host = localhost\n"
        "port = 5432\n"
        "; another comment\n"
        "[flags]\n"
        "verbose = true\n"
        "cache = off\n";

    Config c;
    std::istringstream in(cfg);
    assert(c.load(in) == std::nullopt);

    assert(c.get_string("name") == "CPP Mastery");
    assert(c.get_string("spaced_key") == "spaced value");     // trimmed both sides
    assert(c.get_string("db.host") == "localhost");
    assert(c.get_int("db.port") == 5432);
    assert(c.get_int("db.port", 1) == 5432);
    assert(c.get_int("db.missing", 8080) == 8080);            // default
    assert(c.get_int("name", -1) == -1);                      // "CPP Mastery" is not an int
    assert(c.get_bool("flags.verbose") == true);
    assert(c.get_bool("flags.cache") == false);               // "off"
    assert(c.get_bool("flags.missing", true) == true);

    // round-trip: parse -> serialize -> parse -> same values
    Config c2;
    std::istringstream in2(c.serialize());
    assert(c2.load(in2) == std::nullopt);
    assert(c2.size() == c.size());
    assert(c2.get_string("db.host") == "localhost" && c2.get_int("db.port") == 5432);

    // error reporting with line numbers
    {
        Config e;
        std::istringstream bad("ok = 1\nbroken line without eq\n");
        const auto err = e.load(bad);
        assert(err.has_value() && err->line == 2);
    }
    {
        Config e;
        std::istringstream bad("[unterminated\n");
        const auto err = e.load(bad);
        assert(err.has_value() && err->line == 1);
    }

    std::puts("06_config_parser: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - trim() returns a string_view into the caller's buffer -> zero copy, but
//     the caller must outlive it (here `raw` does). Convert to std::string only
//     when storing.
//   - Typed getters degrade to a default on a missing key OR a bad parse;
//     get_int uses std::from_chars and rejects trailing junk (p must reach end).
//   - Errors carry the line number: "line 2: expected '='" beats "parse error".
//   - Round-trip (parse->serialize->parse) is the cheapest correctness test
//     for any format (49/04 guidelines).
// ============================================================
