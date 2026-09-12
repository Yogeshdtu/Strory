// 05_kv_store.cpp  --  49-PROJECTS intermediate P5
// ============================================================
// Persistent key-value store: put / get / del / scan(prefix).
// Durability = an append-only write-ahead log (length-prefixed records
// with a checksum). Startup replays the log. Compaction rewrites the log
// with only live keys and atomically swaps it in (std::rename).
// Uses <cstdio> (fopen/fread/fwrite/rename/remove) -- no <filesystem>.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 05_kv_store.cpp -o t && ./t
// ============================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

// 32-bit FNV-1a checksum to detect a torn tail record.
std::uint32_t fnv1a(const std::string& s) {
    std::uint32_t h = 2166136261u;
    for (char ch : s) { h ^= static_cast<unsigned char>(ch); h *= 16777619u; }
    return h;
}

// RAII wrapper for a C FILE*.
class File {
public:
    File(const char* path, const char* mode) : f_(std::fopen(path, mode)) {}
    ~File() { if (f_) std::fclose(f_); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    explicit operator bool() const { return f_ != nullptr; }
    std::FILE* get() const { return f_; }
private:
    std::FILE* f_ = nullptr;
};

class KvStore {
public:
    explicit KvStore(std::string log_path) : path_(std::move(log_path)) { replay(); }

    void put(const std::string& k, const std::string& v) { append('p', k, v); map_[k] = v; }
    void del(const std::string& k) {
        if (!map_.count(k)) return;
        append('d', k, "");
        map_.erase(k);
    }
    std::optional<std::string> get(const std::string& k) const {
        const auto it = map_.find(k);
        return it == map_.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
    std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix) const {
        std::vector<std::pair<std::string, std::string>> out;
        for (auto it = map_.lower_bound(prefix); it != map_.end(); ++it) {
            if (it->first.compare(0, prefix.size(), prefix) != 0) break;   // map is ordered
            out.push_back(*it);
        }
        return out;
    }
    std::size_t    size() const { return map_.size(); }
    long           log_bytes() const {
        File f(path_.c_str(), "rb");
        if (!f) return 0;
        std::fseek(f.get(), 0, SEEK_END);
        return std::ftell(f.get());
    }

    // Rewrite the log with only live keys, then atomically replace.
    void compact() {
        const std::string tmp = path_ + ".compact";
        {
            File f(tmp.c_str(), "wb");
            assert(f);
            for (const auto& [k, v] : map_) write_record(f.get(), 'p', k, v);
        }
        std::remove(path_.c_str());                 // Windows rename won't clobber
        std::rename(tmp.c_str(), path_.c_str());    // atomic on POSIX; best-effort here
    }

private:
    void append(char op, const std::string& k, const std::string& v) {
        File f(path_.c_str(), "ab");
        if (f) write_record(f.get(), op, k, v);
    }
    static void write_record(std::FILE* f, char op, const std::string& k, const std::string& v) {
        const std::string payload = std::string(1, op) + k + '\0' + v;
        const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
        const std::uint32_t crc = fnv1a(payload);
        std::fwrite(&len, 4, 1, f);
        std::fwrite(&crc, 4, 1, f);
        std::fwrite(payload.data(), 1, payload.size(), f);
    }
    void replay() {
        map_.clear();
        File f(path_.c_str(), "rb");
        if (!f) return;
        std::uint32_t len = 0, crc = 0;
        while (std::fread(&len, 4, 1, f.get()) == 1 && std::fread(&crc, 4, 1, f.get()) == 1) {
            std::string payload(len, '\0');
            if (std::fread(payload.data(), 1, len, f.get()) != len) break;   // torn tail
            if (fnv1a(payload) != crc || payload.empty())               break;   // corrupt
            const char op = payload[0];
            const auto nul = payload.find('\0', 1);
            if (nul == std::string::npos) break;
            const std::string key = payload.substr(1, nul - 1);
            const std::string val = payload.substr(nul + 1);
            if      (op == 'p') map_[key] = val;
            else if (op == 'd') map_.erase(key);
        }
    }

    std::string                        path_;
    std::map<std::string, std::string> map_;
};

} // namespace

int main() {
    const std::string log = "cppm_49p5_kv.log";
    std::remove(log.c_str());

    // session 1: write some data
    {
        KvStore db(log);
        db.put("user:1", "asha");
        db.put("user:2", "bilal");
        db.put("user:3", "chetna");
        db.put("cfg:theme", "dark");
        db.del("user:2");
        db.put("user:1", "asha2");           // overwrite

        assert(db.get("user:1").value() == "asha2");
        assert(db.get("user:2") == std::nullopt);
        assert(db.size() == 3);

        const auto users = db.scan("user:");
        assert(users.size() == 2);
        assert(users[0].first == "user:1" && users[1].first == "user:3");
    }

    // session 2: reopen -> replay the log -> same state
    long before = 0, after = 0;
    {
        KvStore db(log);
        assert(db.size() == 3);
        assert(db.get("user:1").value() == "asha2");
        assert(db.get("user:2") == std::nullopt);          // the delete replayed
        assert(db.get("cfg:theme").value() == "dark");

        before = db.log_bytes();
        db.compact();
        after = db.log_bytes();
    }
    assert(after > 0 && after < before);                   // dead records dropped

    // session 3: reopen the COMPACTED log -> still correct
    {
        KvStore db(log);
        assert(db.size() == 3);
        assert(db.get("user:1").value() == "asha2");
        assert(db.get("user:3").value() == "chetna");
    }

    // torn-tail: append a header claiming 200 payload bytes but write only 5;
    // reopen -> replay stops at the torn record, the good prefix survives
    {
        {
            File f(log.c_str(), "ab");
            assert(f);
            const std::uint32_t bogus_len = 200, bogus_crc = 0;
            std::fwrite(&bogus_len, 4, 1, f.get());
            std::fwrite(&bogus_crc, 4, 1, f.get());
            std::fwrite("short", 1, 5, f.get());
        }
        KvStore db(log);
        assert(db.size() == 3);
        assert(db.get("user:1").value() == "asha2");
    }

    std::remove(log.c_str());
    std::puts("05_kv_store: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Write-ahead log: every mutation is appended (len + crc + payload) BEFORE
//     the in-memory map is updated. On startup, replay() rebuilds the map.
//     "The log is the source of truth, the map is a cache" -- same idea as
//     inventory P1 and the deterministic engine in 43/44.
//   - The length prefix + CRC let replay detect and drop a torn final record
//     (crash-during-write) without corrupting the DB.
//   - compact(): write live keys to a temp file, then rename() over the
//     original. On POSIX that swap is atomic (a crash leaves the old OR the
//     new log, never a half-written one). Windows needs remove()+rename().
//   - scan() uses std::map ordering + lower_bound: O(log n + matches).
//   - File is a 3-line RAII wrapper -- fclose runs on every path (17).
// ============================================================
