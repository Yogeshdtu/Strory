#include <cstdio>

const char* from_a();
const char* from_b();

int main() {
    std::printf("%s %s\n", from_a(), from_b());
}
