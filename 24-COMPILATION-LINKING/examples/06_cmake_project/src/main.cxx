#include "engine.hpp"
#include <cstdio>

int main() {
    std::printf("flavor: %s\n", engine::build_flavor());
    std::printf("%s\n", engine::shout("hello cmake").c_str());
    return 0;
}
