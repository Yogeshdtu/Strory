// logger.cxx
#include "config.hpp"
#include <cstdio>

// ---------- BUGGY ----------
BadLogger::BadLogger() : tag("ready") {
    std::printf("  BadLogger::BadLogger()  (ab log() safe)\n");
}
void BadLogger::log(const std::string& s) {
    ++lines;
    std::printf("  BADLOG[%d/%s]: %s\n", lines, tag.c_str(), s.c_str());
}
BadLogger g_bad_logger;                  // init order vs g_bad_config: UNSPECIFIED

// ---------- FIXED ----------
void Logger::log(const std::string& s) {
    ++lines;
    std::printf("  LOG[%d]: %s\n", lines, s.c_str());
}
Logger& logger() {
    static Logger instance;              // pehli call pe init (thread-safe guard)
    return instance;
}
