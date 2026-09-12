// config.cxx
#include "config.hpp"
#include <cstdio>

// ---------- BUGGY ----------
BadConfig::BadConfig() : name("engine") {
    // g_bad_logger ka lifetime shuru hua ya nahi? Doosri TU ka global.
    // Agar linker ne is TU ko pehle initialize kiya -> g_bad_logger.tag
    // ka std::string abhi construct nahi hua -> log() UB (crash / garbage).
    g_bad_logger.log("BadConfig ctor: " + name);
}
BadConfig g_bad_config;                  // init order vs g_bad_logger: UNSPECIFIED

// ---------- FIXED ----------
Config::Config() : name("engine"), level(3) {
    logger().log("Config ctor: " + name);   // logger() -> uski static pehle init hoti
}
Config& config() {
    static Config instance;             // ctor ke andar logger() -> ordered chain
    return instance;
}
