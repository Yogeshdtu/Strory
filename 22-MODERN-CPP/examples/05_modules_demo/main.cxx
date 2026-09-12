// main.cxx -- consumer of the `geometry` module
// ============================================================
//   g++ -std=c++20 -fmodules-ts main.cxx geometry.o -o modules_demo
// ============================================================
//   `import geometry;` brings in ONLY the exported names (Point, distance,
//   norm, cosAngle). `dot` is module-private -> not visible here.
// ============================================================

import geometry;
#include <cstdio>          // classic #include still works alongside imports

int main() {
    Point a{3, 4}, b{0, 0}, c{6, 8};

    std::printf("distance(a,b) = %.4f   (expect 5)\n", distance(a, b));
    std::printf("norm(a)       = %.4f   (expect 5)\n", norm(a));
    std::printf("cosAngle(a,c) = %.4f   (expect 1 -- same direction)\n", cosAngle(a, c));

    // double d = dot(a, b);   // ERROR: `dot` is not exported by module geometry

    std::printf(
        "\n"
        "  A module: `export module geometry;` in the interface unit; `import geometry;`\n"
        "  in the consumer. Only `export`ed names are visible. Differences from #include:\n"
        "   - the interface is PARSED ONCE into a binary module (.gcm), not re-parsed per TU\n"
        "   - no textual inclusion -> no macro leakage, no include-order fragility, no ODR\n"
        "     traps from headers seen differently in different TUs\n"
        "   - internal names (`dot`) stay private even without an anonymous namespace\n"
        "  Build impact: faster incremental builds for large codebases, but toolchain\n"
        "  support and build-system integration are still maturing.\n");
    return 0;
}
