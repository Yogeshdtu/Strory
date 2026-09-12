#include "greet.hpp"
#include <cstdio>

int main() {
    std::printf("%s\n", greet::hello());
    int a = greet::bump();
    int b = greet::bump();
    int c = greet::bump();
    std::printf("bump: %d %d %d\n", a, b, c);
    return 0;
}
