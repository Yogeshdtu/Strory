// 05_custom_deleter.cpp
// ============================================================
// Custom deleters -- C APIs (FILE*, sockets, handles) ko RAII mein wrap karna
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 05_custom_deleter.cpp -o cd && ./cd
// ============================================================
//   unique_ptr<T, Deleter> -- Deleter type ka HISSA hai.
//   - stateless function-object deleter -> EBO -> sizeof still 8
//   - function pointer deleter          -> sizeof 16 (pointer store karna padta)
//   - capturing lambda deleter          -> sizeof 8 + capture size
//   shared_ptr<T> -- deleter ALWAYS control block mein -> shared_ptr size unchanged (16)
// ============================================================

#include <cstdio>
#include <iostream>
#include <memory>

// ---- 1. stateless deleter struct (best -- zero size overhead) ----
struct FileCloser {
    void operator()(std::FILE* f) const noexcept {
        if (f) { std::puts("  FileCloser: fclose"); std::fclose(f); }
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

FilePtr openFile(const char* path, const char* mode) {
    return FilePtr{std::fopen(path, mode)};       // fopen -> unique_ptr; scope end -> fclose
}

// ---- generic C-handle wrapper via a deleter function ----
extern "C" void fakeSocketClose(int* fd) {
    if (fd && *fd >= 0) { std::cout << "  fakeSocketClose(fd=" << *fd << ")\n"; *fd = -1; }
}

int main() {
    std::cout << "=== 1. FILE* wrapped in unique_ptr (custom deleter) ===\n";
    {
        FilePtr fp = openFile("raii_demo.txt", "w");
        if (fp) {
            std::fputs("hello from RAII FILE*\n", fp.get());
            std::cout << "  wrote to file; leaving scope...\n";
        }
    }   // FileCloser called -> fclose  (koi manual fclose nahi)
    std::remove("raii_demo.txt");

    std::cout << "\n=== 2. deleter type -> unique_ptr size ===\n";
    auto lambdaDel = [](std::FILE* f) { if (f) std::fclose(f); };          // stateless lambda
    std::cout << "  unique_ptr<FILE, FileCloser>            : "
              << sizeof(std::unique_ptr<std::FILE, FileCloser>) << "  (stateless struct -> EBO -> 8)\n";
    std::cout << "  unique_ptr<FILE, decltype(lambdaDel)>   : "
              << sizeof(std::unique_ptr<std::FILE, decltype(lambdaDel)>) << "  (stateless lambda -> 8)\n";
    std::cout << "  unique_ptr<FILE, void(*)(FILE*)>        : "
              << sizeof(std::unique_ptr<std::FILE, void(*)(std::FILE*)>) << "  (fn pointer -> stored -> 16)\n";
    std::cout << "  shared_ptr<FILE> (any deleter)          : "
              << sizeof(std::shared_ptr<std::FILE>) << "  (deleter in control block -> unchanged)\n";

    std::cout << "\n=== 3. shared_ptr with a custom deleter (lambda) ===\n";
    {
        std::shared_ptr<std::FILE> sfp(std::fopen("raii_demo2.txt", "w"),
                                       [](std::FILE* f) {
                                           std::cout << "  shared_ptr deleter: fclose\n";
                                           if (f) std::fclose(f);
                                       });
        if (sfp) std::fputs("shared file\n", sfp.get());
    }   // deleter runs at refcount 0
    std::remove("raii_demo2.txt");

    std::cout << "\n=== 4. wrapping a C-style handle (fd) ===\n";
    {
        int fd = 7;                                                   // pretend socket fd
        std::unique_ptr<int, void(*)(int*)> sock{&fd, &fakeSocketClose};
        std::cout << "  using socket fd=" << *sock << "\n";
    }   // fakeSocketClose(&fd) -> fd = -1

    std::cout <<
        "\n"
        "  Pattern: har C 'open/close' pair ko unique_ptr<T, Closer> se wrap karo.\n"
        "  Stateless deleter struct -> EBO -> zero size overhead. Function-pointer\n"
        "  deleter -> +8 bytes. shared_ptr -> deleter hamesha control block mein.\n";
    return 0;
}
