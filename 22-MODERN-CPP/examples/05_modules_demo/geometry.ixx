// geometry.ixx -- a C++20 module INTERFACE unit
// ============================================================
//   Built separately (NOT via ./build.ps1 folder). See build.ps1 / build.sh here.
//   g++ -std=c++20 -fmodules-ts -c geometry.ixx -o geometry.o
// ============================================================

module;                          // --- global module fragment ---
#include <cmath>                  // classic includes go here (before `export module`)

export module geometry;          // this file defines the `geometry` module

// --- exported: visible to code that `import geometry;` ---
export struct Point {
    double x = 0, y = 0;
};

export double distance(Point a, Point b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

export inline double norm(Point p) { return distance({0, 0}, p); }

// --- NOT exported: internal to the module, invisible to importers ---
double dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }   // no `export` -> module-private

export double cosAngle(Point a, Point b) {
    double na = norm(a), nb = norm(b);
    return (na == 0 || nb == 0) ? 0.0 : dot(a, b) / (na * nb);   // uses the private dot()
}
