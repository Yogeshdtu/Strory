// main.cxx — dono side dikhata hai
#include "config.hpp"
#include <cstdio>

int main() {
    std::puts("== BUGGY (raw cross-TU globals) ==");
    std::printf("  g_bad_config.name = %s\n", g_bad_config.name.c_str());
    std::printf("  g_bad_logger.lines = %d\n", g_bad_logger.lines);
    std::puts("  ^ agar upar 'BADLOG' print hua g_bad_logger ctor SE PEHLE,");
    std::puts("    ya crash hua, to fiasco reproduce ho gaya (link order pe depend).");

    std::puts("\n== FIXED (construct-on-first-use) ==");
    std::printf("  config().name = %s level=%d\n", config().name.c_str(), config().level);
    std::printf("  logger().lines = %d  (config ctor ne 1 line likhi)\n", logger().lines);
    std::puts("  ^ config()'s static ctor ne logger() call kiya -> logger ki static");
    std::puts("    PEHLE fully-constructed hui. Order hamesha sahi.");
    return 0;
}
