// 05_static_members.cpp
// ============================================================
// static data members + static member functions
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_static_members.cpp -o sm && ./sm
// ============================================================
//   static member  = class ka, kisi ek object ka NAHI. Ek hi copy, sab objects share.
//   static method  = class-level function; koi `this` nahi; sirf static members
//                    (aur passed args) tak pahunch.
//
//   C++17: `inline static` -> class ke andar hi define, alag .cpp line nahi chahiye.
// ============================================================

#include <iostream>
#include <string>

class Connection {
    // ---- per-object (instance) data ----
    int         id_;
    std::string host_;

    // ---- class-wide (static) data ----
    static inline int  s_liveCount = 0;      // C++17 inline static -> yahin define
    static inline int  s_totalEver = 0;
    static inline int  s_nextId    = 1;

public:
    explicit Connection(std::string host)
        : id_(s_nextId++), host_(std::move(host)) {
        ++s_liveCount;
        ++s_totalEver;
        std::cout << "  open  #" << id_ << " -> " << host_
                  << "   (live=" << s_liveCount << ")\n";
    }

    ~Connection() {
        --s_liveCount;
        std::cout << "  close #" << id_ << "   (live=" << s_liveCount << ")\n";
    }

    Connection(const Connection&)            = delete;   // simplicity: no copy
    Connection& operator=(const Connection&) = delete;

    int id() const { return id_; }

    // ---- static member functions -- object ke bina call ho sakti ----
    static int  liveCount()  { return s_liveCount; }     // no `this`
    static int  totalEver()  { return s_totalEver; }
    static bool atCapacity() { return s_liveCount >= 3; }
};

int main() {
    std::cout << "start: live=" << Connection::liveCount()      // ClassName::method()
              << "  atCapacity=" << std::boolalpha << Connection::atCapacity() << "\n\n";

    Connection a{"10.0.0.1"};
    Connection b{"10.0.0.2"};
    {
        Connection c{"10.0.0.3"};
        std::cout << "\n  inside block: live=" << Connection::liveCount()
                  << "  atCapacity=" << Connection::atCapacity() << "\n\n";
    }   // c destroyed here

    std::cout << "\nafter block: live=" << Connection::liveCount()
              << "  totalEver=" << Connection::totalEver() << "\n";

    // static method object se bhi call ho sakti (par ClassName:: clearer)
    std::cout << "a.liveCount() bhi chalta: " << a.liveCount() << "\n";

    std::cout <<
        "\n"
        "  static data  -> ek copy, sab objects share (yahan: live/total/nextId counters)\n"
        "  static method -> no `this`, sirf static members + args\n"
        "  access: ClassName::member  (object ki zaroorat nahi)\n"
        "  C++17 `inline static` -> class body mein hi initialize\n";
    return 0;
}
