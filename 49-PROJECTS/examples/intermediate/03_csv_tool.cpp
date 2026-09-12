// 03_csv_tool.cpp  --  49-PROJECTS intermediate P3
// ============================================================
// RFC-4180-ish CSV: a state-machine field parser (quoted fields, embedded
// commas / newlines / doubled quotes), a header row, then filter / sort /
// aggregate over the rows. Parser reports the byte offset on an error.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 03_csv_tool.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Row  = std::vector<std::string>;

struct ParseErr { std::size_t offset; std::string msg; };

// State machine: FieldStart -> Unquoted | Quoted -> (QuoteInQuoted) -> ...
std::optional<ParseErr> parse_csv(std::string_view in, std::vector<Row>& out) {
    out.clear();
    Row row;
    std::string field;
    enum class St { FieldStart, Unquoted, Quoted, QuoteInQuoted } st = St::FieldStart;
    bool row_has_data = false;

    auto end_field = [&] { row.push_back(std::move(field)); field.clear(); row_has_data = true; };
    auto end_row   = [&] { end_field(); out.push_back(std::move(row)); row.clear(); row_has_data = false; };

    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        switch (st) {
            case St::FieldStart:
                if      (c == '"')  st = St::Quoted;
                else if (c == ',')  { end_field(); }
                else if (c == '\n') { end_row(); }
                else if (c == '\r') { /* skip, expect \n next */ }
                else { field.push_back(c); st = St::Unquoted; }
                break;
            case St::Unquoted:
                if      (c == ',')  { end_field(); st = St::FieldStart; }
                else if (c == '\n') { end_row();   st = St::FieldStart; }
                else if (c == '\r') { /* skip */ }
                else if (c == '"')  return ParseErr{i, "unexpected '\"' in an unquoted field"};
                else field.push_back(c);
                break;
            case St::Quoted:
                if (c == '"') st = St::QuoteInQuoted;
                else field.push_back(c);                 // commas, newlines are literal here
                break;
            case St::QuoteInQuoted:
                if      (c == '"')  { field.push_back('"'); st = St::Quoted; }   // "" -> "
                else if (c == ',')  { end_field(); st = St::FieldStart; }
                else if (c == '\n') { end_row();   st = St::FieldStart; }
                else if (c == '\r') { /* skip */ }
                else return ParseErr{i, "text after a closing quote"};
                break;
        }
    }
    if (st == St::Quoted) return ParseErr{in.size(), "unterminated quoted field"};
    if (row_has_data || !field.empty() || st == St::QuoteInQuoted) end_row();  // last row w/o trailing \n
    return std::nullopt;
}

class Table {
public:
    std::optional<ParseErr> load(std::string_view csv) {
        std::vector<Row> rows;
        if (auto e = parse_csv(csv, rows)) return e;
        if (rows.empty()) return std::nullopt;
        header_ = std::move(rows.front());
        data_.assign(rows.begin() + 1, rows.end());
        return std::nullopt;
    }
    std::size_t col(std::string_view name) const {
        for (std::size_t i = 0; i < header_.size(); ++i) if (header_[i] == name) return i;
        return static_cast<std::size_t>(-1);
    }
    std::size_t rows() const { return data_.size(); }
    const std::string& at(std::size_t r, std::size_t c) const { return data_[r][c]; }

    template <class Pred>
    Table where(Pred p) const {
        Table t; t.header_ = header_;
        for (const auto& r : data_) if (p(r)) t.data_.push_back(r);
        return t;
    }
    void sort_by(std::size_t c, bool desc = false) {
        std::stable_sort(data_.begin(), data_.end(), [c, desc](const Row& a, const Row& b) {
            double x = 0, y = 0;
            const bool an = to_num(a[c], x), bn = to_num(b[c], y);
            if (an && bn) return desc ? (x > y) : (x < y);
            return desc ? (a[c] > b[c]) : (a[c] < b[c]);          // fall back to lexicographic
        });
    }
    double sum(std::size_t c) const {
        double s = 0;
        for (const auto& r : data_) { double v = 0; if (to_num(r[c], v)) s += v; }
        return s;
    }

    static bool to_num(const std::string& s, double& out) {
        const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        return ec == std::errc{} && p == s.data() + s.size();
    }

private:
    Row              header_;
    std::vector<Row> data_;
};

} // namespace

int main() {
    // every quoting edge case
    const std::string csv =
        "name,city,marks\n"
        "Asha,\"New Delhi, DL\",88\n"
        "\"Bilal \"\"B\"\"\",Mumbai,72\n"
        "Chetna,\"line1\nline2\",95\n";

    Table t;
    const auto err = t.load(csv);
    assert(!err.has_value());
    assert(t.rows() == 3);

    const auto cName = t.col("name"), cCity = t.col("city"), cMarks = t.col("marks");
    assert(cName == 0 && cCity == 1 && cMarks == 2);

    assert(t.at(0, cCity) == "New Delhi, DL");        // embedded comma preserved
    assert(t.at(1, cName) == "Bilal \"B\"");          // "" -> " unescaped
    assert(t.at(2, cCity) == "line1\nline2");         // embedded newline preserved

    // filter: marks > 80
    Table hi = t.where([cMarks](const Row& r) {
        double m = 0; return Table::to_num(r[cMarks], m) && m > 80.0;
    });
    assert(hi.rows() == 2);                           // Asha 88, Chetna 95

    // sort by marks desc
    hi.sort_by(cMarks, /*desc*/true);
    assert(hi.at(0, cName) == "Chetna" && hi.at(1, cName) == "Asha");

    // aggregate
    assert(std::abs(t.sum(cMarks) - (88 + 72 + 95)) < 1e-9);

    // errors carry an offset
    {
        std::vector<Row> junk;
        const auto e = parse_csv("a,\"unterminated", junk);
        assert(e.has_value());
    }
    {
        std::vector<Row> junk;
        const auto e = parse_csv("a,b\"c,d\n", junk);   // quote mid unquoted field
        assert(e.has_value() && e->offset == 3);
    }

    std::puts("03_csv_tool: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - A 4-state machine, NOT split(','): only this handles "a,b", "he said
//     ""hi""", and fields with newlines correctly.
//   - header row -> name->index map; queries take column indices.
//   - sort_by tries numeric first, falls back to lexicographic -> mixed columns
//     don't crash. stable_sort keeps equal rows in file order.
//   - Errors report the byte offset -> "col 3: unexpected quote" beats a bool.
//   - Extension: stream the parse (don't hold the whole file); this design
//     already processes char-by-char so it's a small change.
// ============================================================
