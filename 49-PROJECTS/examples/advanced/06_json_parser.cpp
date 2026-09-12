// 06_json_parser.cpp  --  49-PROJECTS advanced P6
// ============================================================
// Recursive-descent JSON parser (RFC 8259 subset). Builds a Value tree
// (null | bool | double | string | array | object). Precise errors with
// line:col. Depth-limited (attacker input + recursion = stack overflow).
// Serializer + round-trip test.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 06_json_parser.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace json {

struct Value;
using Array  = std::vector<Value>;
using Object = std::map<std::string, Value>;

struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v{nullptr};

    bool is_null()   const { return std::holds_alternative<std::nullptr_t>(v); }
    bool as_bool()   const { return std::get<bool>(v); }
    double as_num()  const { return std::get<double>(v); }
    const std::string& as_str() const { return std::get<std::string>(v); }
    const Array&  as_arr() const { return std::get<Array>(v); }
    const Object& as_obj() const { return std::get<Object>(v); }
};

struct ParseError : std::runtime_error {
    int line, col;
    ParseError(int l, int c, const std::string& m)
        : std::runtime_error("line " + std::to_string(l) + " col " + std::to_string(c) + ": " + m),
          line(l), col(c) {}
};

class Parser {
public:
    explicit Parser(std::string_view s, int max_depth = 200) : s_(s), max_depth_(max_depth) {}

    Value parse() {
        skip_ws();
        Value v = parse_value(0);
        skip_ws();
        if (i_ != s_.size()) fail("trailing characters after JSON value");
        return v;
    }

private:
    std::string_view s_;
    std::size_t      i_ = 0;
    int              line_ = 1, col_ = 1;
    int              max_depth_;

    [[noreturn]] void fail(const std::string& m) { throw ParseError(line_, col_, m); }

    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    char get() {
        const char c = s_[i_++];
        if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
        return c;
    }
    bool eof() const { return i_ >= s_.size(); }
    void skip_ws() {
        while (!eof()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') get();
            else break;
        }
    }
    void expect(char c) {
        if (eof() || peek() != c) fail(std::string("expected '") + c + "'");
        get();
    }

    Value parse_value(int depth) {
        if (depth > max_depth_) fail("maximum nesting depth exceeded");
        skip_ws();
        if (eof()) fail("unexpected end of input");
        switch (peek()) {
            case '{': return parse_object(depth);
            case '[': return parse_array(depth);
            case '"': return Value{parse_string()};
            case 't': case 'f': return parse_bool();
            case 'n': return parse_null();
            default:  return parse_number();
        }
    }

    Value parse_null() {
        if (s_.substr(i_, 4) != "null") fail("invalid literal");
        for (int k = 0; k < 4; ++k) get();
        return Value{nullptr};
    }
    Value parse_bool() {
        if (s_.substr(i_, 4) == "true")  { for (int k = 0; k < 4; ++k) get(); return Value{true}; }
        if (s_.substr(i_, 5) == "false") { for (int k = 0; k < 5; ++k) get(); return Value{false}; }
        fail("invalid literal");
    }
    Value parse_number() {
        const std::size_t start = i_;
        if (peek() == '-') get();
        if (peek() == '0') { get(); }                       // leading zero: only a lone 0
        else if (peek() >= '1' && peek() <= '9') { while (peek() >= '0' && peek() <= '9') get(); }
        else fail("invalid number");
        if (peek() == '.') { get(); if (!(peek() >= '0' && peek() <= '9')) fail("digit expected after '.'");
                             while (peek() >= '0' && peek() <= '9') get(); }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            if (!(peek() >= '0' && peek() <= '9')) fail("digit expected in exponent");
            while (peek() >= '0' && peek() <= '9') get();
        }
        double out = 0;
        const std::string_view tok = s_.substr(start, i_ - start);
        const auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), out);
        if (ec != std::errc{} || p != tok.data() + tok.size()) fail("malformed number");
        return Value{out};
    }
    std::string parse_string() {
        expect('"');
        std::string out;
        for (;;) {
            if (eof()) fail("unterminated string");
            const char c = get();
            if (c == '"') break;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out.push_back(c); continue; }
            if (eof()) fail("unterminated escape");
            const char e = get();
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u':  append_utf8(out, parse_hex4()); break;
                default:   fail("invalid escape");
            }
        }
        return out;
    }
    unsigned parse_hex4() {
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            if (eof()) fail("truncated \\u escape");
            const char c = get();
            v <<= 4;
            if      (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad hex digit in \\u escape");
        }
        return v;
    }
    static void out_push(std::string& out, unsigned byte) {
        out.push_back(static_cast<char>(byte & 0xFFu));
    }
    static void append_utf8(std::string& out, unsigned cp) {   // BMP only; surrogates left as-is
        if (cp < 0x80u) {
            out_push(out, cp);
        } else if (cp < 0x800u) {
            out_push(out, 0xC0u | (cp >> 6));
            out_push(out, 0x80u | (cp & 0x3Fu));
        } else {
            out_push(out, 0xE0u | (cp >> 12));
            out_push(out, 0x80u | ((cp >> 6) & 0x3Fu));
            out_push(out, 0x80u | (cp & 0x3Fu));
        }
    }

    Value parse_array(int depth) {
        expect('[');
        Array arr;
        skip_ws();
        if (peek() == ']') { get(); return Value{std::move(arr)}; }
        for (;;) {
            arr.push_back(parse_value(depth + 1));
            skip_ws();
            if (peek() == ',') { get(); continue; }
            if (peek() == ']') { get(); break; }
            fail("expected ',' or ']'");
        }
        return Value{std::move(arr)};
    }
    Value parse_object(int depth) {
        expect('{');
        Object obj;
        skip_ws();
        if (peek() == '}') { get(); return Value{std::move(obj)}; }
        for (;;) {
            skip_ws();
            if (peek() != '"') fail("expected string key");
            std::string key = parse_string();
            skip_ws();
            expect(':');
            obj[std::move(key)] = parse_value(depth + 1);
            skip_ws();
            if (peek() == ',') { get(); continue; }
            if (peek() == '}') { get(); break; }
            fail("expected ',' or '}'");
        }
        return Value{std::move(obj)};
    }
};

// --- serializer ---
void dump(const Value& v, std::string& out) {
    struct Vis {
        std::string& o;
        void operator()(std::nullptr_t) const { o += "null"; }
        void operator()(bool b) const { o += b ? "true" : "false"; }
        void operator()(double d) const {
            char buf[32];
            auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), d);
            (void)ec;
            o.append(buf, p);
        }
        void operator()(const std::string& s) const {
            o += '"';
            for (char c : s) switch (c) {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n";  break;
                case '\t': o += "\\t";  break;
                case '\r': o += "\\r";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char b[8];
                        std::snprintf(b, sizeof(b), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                        o += b;
                    } else {
                        o += c;
                    }
            }
            o += '"';
        }
        void operator()(const Array& a) const {
            o += '[';
            for (std::size_t k = 0; k < a.size(); ++k) { if (k) o += ','; dump(a[k], o); }
            o += ']';
        }
        void operator()(const Object& m) const {
            o += '{';
            bool first = true;
            for (const auto& [k, val] : m) {
                if (!first) o += ',';
                first = false;
                (*this)(k);
                o += ':';
                dump(val, o);
            }
            o += '}';
        }
    };
    std::visit(Vis{out}, v.v);
}

std::string dump(const Value& v) { std::string s; dump(v, s); return s; }

Value parse(std::string_view s) { return Parser(s).parse(); }

} // namespace json

int main() {
    using json::parse;
    using json::dump;

    // happy path
    {
        auto v = parse(R"({"name":"Asha","age":30,"scores":[88,92.5,70],"active":true,"tags":null})");
        assert(v.as_obj().at("name").as_str() == "Asha");
        assert(v.as_obj().at("age").as_num() == 30.0);
        assert(v.as_obj().at("scores").as_arr().size() == 3);
        assert(v.as_obj().at("scores").as_arr()[1].as_num() == 92.5);
        assert(v.as_obj().at("active").as_bool() == true);
        assert(v.as_obj().at("tags").is_null());
    }

    // nesting + whitespace + empty containers
    {
        auto v = parse("  {\n  \"a\" : [ {}, [], { \"b\" : [1,2,3] } ]\n}  ");
        assert(v.as_obj().at("a").as_arr().size() == 3);
        assert(v.as_obj().at("a").as_arr()[2].as_obj().at("b").as_arr()[2].as_num() == 3.0);
    }

    // escapes
    {
        auto v = parse(R"("a\tb\nc\"d\\e")");
        assert(v.as_str() == "a\tb\nc\"d\\e");
    }

    // round-trip: parse -> dump -> parse -> structurally equal
    {
        const std::string src = R"({"x":[1,2,3],"y":{"z":true},"s":"hi\nthere"})";
        auto a = parse(src);
        auto b = parse(dump(a));
        assert(dump(a) == dump(b));                       // stable canonical form
        assert(b.as_obj().at("s").as_str() == "hi\nthere");
    }

    // rejects: each must throw a ParseError
    auto rejects = [](std::string_view s) {
        try { parse(s); return false; }
        catch (const json::ParseError&) { return true; }
    };
    assert(rejects("{"));
    assert(rejects("[1,2,]"));                            // trailing comma
    assert(rejects("{\"a\":1,}"));
    assert(rejects("nul"));
    assert(rejects("01"));                                // leading zero
    assert(rejects("1."));                                // no digit after '.'
    assert(rejects("\"unterminated"));
    assert(rejects("{\"a\" 1}"));                         // missing ':'
    assert(rejects("[1,2] garbage"));                     // trailing junk
    assert(rejects("\"\x01\""));                          // control char in string

    // depth limit: a very deep array is rejected cleanly, not a crash
    {
        std::string deep(5000, '[');
        assert(rejects(deep));
        try { parse(deep); }
        catch (const json::ParseError& e) {
            assert(std::string(e.what()).find("nesting depth") != std::string::npos);
        }
    }

    // error carries a position
    {
        try { parse("{\n  \"a\": @\n}"); assert(false); }
        catch (const json::ParseError& e) { assert(e.line == 2); }
    }

    std::puts("06_json_parser: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Recursive descent: one function per grammar production
//     (value/object/array/string/number). The Value tree is a std::variant
//     with std::vector<Value> / std::map<string,Value> members (22).
//   - Depth limiting: parse_value takes a depth and fails past max_depth_.
//     Without it, "[[[[..." of attacker length overflows the C++ stack (11/UB,
//     45). Real parsers do this or switch to an explicit stack.
//   - Errors carry line:col -- get() tracks them as it consumes. "line 2 col 9:
//     expected ',' or '}'" beats "parse error".
//   - Round-trip (parse -> dump -> parse, compare canonical dumps) is the
//     cheapest strong correctness test for any format (49/04).
//   - Numbers validated by the grammar THEN handed to std::from_chars; leading
//     zeros / bare '1.' / '01' are rejected before conversion.
// ============================================================
