// config.hpp — shared interface
// ============================================================
// BUGGY: do raw globals, do alag TUs, ek dusre pe depend.
//        g_bad_config ka ctor g_bad_logger ko use karta hai — par
//        cross-TU init order UNSPECIFIED -> "static initialization
//        order fiasco".
// FIXED: construct-on-first-use (function-local static). Order use
//        se guarantee hota hai.
// ============================================================
#pragma once
#include <string>

// ---------- BUGGY side ----------
struct BadLogger {
    int lines = 0;
    std::string tag;
    BadLogger();                         // logger.cxx
    void log(const std::string& s);      // ++lines; print
};
extern BadLogger g_bad_logger;           // logger.cxx mein defined

struct BadConfig {
    std::string name;
    BadConfig();                         // config.cxx — g_bad_logger use karta hai (!)
};
extern BadConfig g_bad_config;           // config.cxx mein defined

// ---------- FIXED side ----------
struct Logger {
    int lines = 0;
    void log(const std::string& s);
};
struct Config {
    std::string name;
    int level;
    Config();                            // logger() ko call karta hai (ordered)
};
Logger& logger();                        // function-local static
Config& config();                        // "        "        , ctor -> logger()
