// 04_log_analyzer.cpp  --  49-PROJECTS intermediate P4
// ============================================================
// Parse "epoch_sec LEVEL source message" log lines. Report: per-level
// counts, per-minute error buckets, top-N error messages, a spike flag
// (a minute whose error count exceeds k x the running baseline).
// Malformed lines are counted and skipped -- never crash on bad input.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 04_log_analyzer.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <istream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum class Level { Debug, Info, Warn, Error, Unknown };

Level parse_level(std::string_view s) {
    if (s == "DEBUG") return Level::Debug;
    if (s == "INFO")  return Level::Info;
    if (s == "WARN")  return Level::Warn;
    if (s == "ERROR") return Level::Error;
    return Level::Unknown;
}

struct Report {
    std::array<std::uint64_t, 5> level_count{};        // indexed by Level
    std::map<std::int64_t, std::uint64_t> errors_per_minute;
    std::unordered_map<std::string, std::uint64_t> error_messages;
    std::uint64_t malformed = 0;
    std::vector<std::int64_t> spike_minutes;
};

Report analyze(std::istream& in, double spike_factor = 3.0) {
    Report r;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::int64_t epoch = 0;
        std::string  lvl, source, rest;
        if (!(ls >> epoch >> lvl >> source)) { ++r.malformed; continue; }
        std::getline(ls, rest);                        // the message (may be empty)
        if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);

        const Level L = parse_level(lvl);
        r.level_count[static_cast<std::size_t>(L)]++;

        if (L == Level::Error) {
            const std::int64_t minute = epoch / 60;
            r.errors_per_minute[minute]++;
            r.error_messages[rest]++;
        }
    }

    // spike detection: walk minutes in order, keep a running mean of prior minutes
    double running_sum = 0.0;
    std::uint64_t n = 0;
    for (const auto& [minute, count] : r.errors_per_minute) {
        if (n > 0) {
            const double baseline = running_sum / static_cast<double>(n);
            if (static_cast<double>(count) > spike_factor * std::max(baseline, 1.0))
                r.spike_minutes.push_back(minute);
        }
        running_sum += static_cast<double>(count);
        ++n;
    }
    return r;
}

std::vector<std::pair<std::string, std::uint64_t>>
top_errors(const Report& r, std::size_t k) {
    std::vector<std::pair<std::string, std::uint64_t>> v(r.error_messages.begin(), r.error_messages.end());
    if (k > v.size()) k = v.size();
    std::partial_sort(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end(),
                      [](const auto& a, const auto& b) {
                          if (a.second != b.second) return a.second > b.second;
                          return a.first < b.first;
                      });
    v.resize(k);
    return v;
}

std::uint64_t at(const Report& r, Level L) { return r.level_count[static_cast<std::size_t>(L)]; }

} // namespace

int main() {
    const char* log =
        "0 INFO web started\n"
        "5 DEBUG web handling /\n"
        "10 WARN db slow query\n"
        "20 ERROR db connection refused\n"
        "25 ERROR web timeout\n"
        "70 ERROR db connection refused\n"          // minute 1
        "80 INFO web ok\n"
        "this line is garbage\n"                     // malformed
        "900 ERROR db connection refused\n"          // minute 15
        "905 ERROR db connection refused\n"
        "906 ERROR db connection refused\n"
        "907 ERROR db connection refused\n"
        "908 ERROR db connection refused\n";         // minute 15 has 5 -> spike

    std::istringstream in(log);
    const Report r = analyze(in, /*spike_factor*/3.0);

    assert(at(r, Level::Info)  == 2);      // lines "0 INFO ..." and "80 INFO ..."
    assert(at(r, Level::Debug) == 1);
    assert(at(r, Level::Warn)  == 1);
    assert(at(r, Level::Error) == 8);
    assert(r.malformed == 1);

    // errors per minute: minute 0 -> 2, minute 1 -> 1, minute 15 -> 5
    assert(r.errors_per_minute.at(0)  == 2);
    assert(r.errors_per_minute.at(1)  == 1);
    assert(r.errors_per_minute.at(15) == 5);

    // top error message (source is a separate field; message is what's left)
    const auto te = top_errors(r, 2);
    assert(te.size() == 2);
    assert(te[0].first == "connection refused" && te[0].second == 7);
    assert(te[1].first == "timeout" && te[1].second == 1);

    // spike: minute 15 (count 5) vs baseline mean of minutes {0,1} = (2+1)/2 = 1.5
    //   5 > 3.0 * max(1.5, 1.0) = 4.5  -> flagged
    assert(r.spike_minutes.size() == 1 && r.spike_minutes.front() == 15);

    // a flat log flags nothing
    {
        std::istringstream flat("0 ERROR a x\n60 ERROR a x\n120 ERROR a x\n180 ERROR a x\n");
        const Report fr = analyze(flat);
        assert(fr.spike_minutes.empty());
    }

    std::puts("04_log_analyzer: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Robust parsing: a line that doesn't start "int LEVEL source ..." is
//     counted as malformed and skipped. The analyzer never throws on input.
//   - errors_per_minute is a std::map -> ordered iteration for the time-series
//     spike walk. top_errors uses partial_sort + tie-break.
//   - Spike detection is a running-mean baseline (a sliding-window idea, 20);
//     max(baseline, 1.0) stops a divide-by-tiny false positive on quiet logs.
//   - Extension: percentiles from a duration field -> the histogram in 47/10 #10.
// ============================================================
