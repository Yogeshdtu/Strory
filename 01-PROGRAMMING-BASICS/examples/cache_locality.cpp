// cache_locality.cpp
// ============================================================
// LESSON 11 ka example: cache locality ka asli asar
// ============================================================
// YEH SABSE IMPORTANT EXAMPLE HAI IS FOLDER KA.
//
// Do loops. Bilkul same kaam. Bilkul same O(n^2) complexity.
// Bas memory access ka order alag.
// Result: 3-10x speed difference.
//
// Yehi poori performance engineering ka core idea hai.
// ============================================================
//   g++ -std=c++20 -O2 cache_locality.cpp -o cache_locality && ./cache_locality
//
// -O2 zaroori hai! -O0 pe result bekaar aayega kyunki loop overhead
// asli effect ko chhupa dega.
// ============================================================

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

int main() {
    // N x N ka matrix. 2048 x 2048 x 4 bytes = ~16 MB.
    // Yeh L3 cache se bada hai (usually 8-32 MB), isliye effect saaf dikhega.
    const int N = 2048;

    std::cout << "Matrix bana rahe hain: " << N << " x " << N
              << " (" << (static_cast<long long>(N) * N * sizeof(int)) / (1024 * 1024)
              << " MB)\n\n";

    // Ek hi flat vector use kar rahe hain (vector<vector<int>> nahi),
    // taaki memory ek continuous block mein ho -- exactly jaise ek 2D array hota hai.
    // Row-major layout: element (i, j) index (i * N + j) pe hai.
    std::vector<int> matrix(static_cast<std::size_t>(N) * N, 1);

    // =========================================================
    // TEST 1: ROW-WISE traversal (CACHE FRIENDLY)
    // =========================================================
    // Hum j ko andar ke loop mein badha rahe hain.
    // Iska matlab: consecutive memory addresses access ho rahe hain.
    //
    // Memory mein:  [i=0,j=0][i=0,j=1][i=0,j=2] ... <- ek hi cache line mein 16 ints
    //
    // Ek cache miss ke baad agle 15 access FREE (cache hit) hain.
    auto t1 = std::chrono::steady_clock::now();
    long long sumRow = 0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            sumRow += matrix[static_cast<std::size_t>(i) * N + j];
        }
    }
    auto t2 = std::chrono::steady_clock::now();

    // =========================================================
    // TEST 2: COLUMN-WISE traversal (CACHE UNFRIENDLY)
    // =========================================================
    // Ab i andar ke loop mein hai.
    // Har step mein hum N * 4 = 8192 bytes aage jump kar rahe hain.
    //
    // Memory mein:  [i=0,j=0] ... 8192 bytes ka jump ... [i=1,j=0]
    //
    // HAR access ek naya cache line = HAR access ek cache miss.
    // Aur cache line ke baaki 60 bytes bekaar chale jaate hain.
    auto t3 = std::chrono::steady_clock::now();
    long long sumCol = 0;
    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            sumCol += matrix[static_cast<std::size_t>(i) * N + j];
        }
    }
    auto t4 = std::chrono::steady_clock::now();

    // =========================================================
    // RESULTS
    // =========================================================
    auto rowMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    auto colMs = std::chrono::duration<double, std::milli>(t4 - t3).count();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Row-wise    (cache FRIENDLY)   : " << rowMs << " ms\n";
    std::cout << "Column-wise (cache UNFRIENDLY) : " << colMs << " ms\n";
    std::cout << "Slowdown                       : " << (colMs / rowMs) << "x\n\n";

    // Sanity check -- dono ka sum same hona chahiye
    std::cout << "Dono sums barabar hain? " << (sumRow == sumCol ? "haan" : "NAHI")
              << "  (sum = " << sumRow << ")\n\n";

    std::cout << "=================================================\n";
    std::cout << "  SAMJHO KYA HUA\n";
    std::cout << "=================================================\n";
    std::cout << "* Dono loops ne EXACTLY same kaam kiya\n";
    std::cout << "* Dono ki complexity O(N^2) hai\n";
    std::cout << "* Dono ne " << (static_cast<long long>(N) * N) << " additions kiye\n";
    std::cout << "* Fark sirf MEMORY ACCESS ORDER ka tha\n\n";
    std::cout << "Big-O ne aapko kuch nahi bataya. Cache ne sab bataya.\n\n";
    std::cout << "Yahi wajah hai ki HFT mein hum data layout pe itna dhyaan dete hain.\n";
    std::cout << "Folder 32 mein iska poora science padhenge.\n";

    return 0;
}
