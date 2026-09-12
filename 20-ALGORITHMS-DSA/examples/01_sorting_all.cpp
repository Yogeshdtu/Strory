// 01_sorting_all.cpp
// ============================================================
// Bubble / insertion / selection / merge / quick / heap sort --
// implement each, verify correctness, and BENCHMARK against std::sort
// ============================================================
//   NORMAL: g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_sorting_all.cpp -o s && ./s
//   BENCH : g++ -std=c++20 -O2 01_sorting_all.cpp -o s && ./s
// ============================================================
//   Takeaways:
//   - O(n^2) sorts (bubble/insertion/selection) are unusable past a few thousand
//   - merge sort: O(n log n) guaranteed, stable, needs O(n) scratch
//   - quicksort: O(n log n) average, O(n^2) worst (bad pivot), in place
//   - heap sort: O(n log n) guaranteed, in place, but cache-unfriendly
//   - std::sort (introsort) beats all hand-written versions
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---------- O(n^2) family ----------
static void bubbleSort(std::vector<int>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        bool swapped = false;
        for (std::size_t j = 0; j + 1 < n - i; ++j)
            if (a[j] > a[j + 1]) { std::swap(a[j], a[j + 1]); swapped = true; }
        if (!swapped) break;                       // already sorted -> early out
    }
}

static void insertionSort(std::vector<int>& a) {
    for (std::size_t i = 1; i < a.size(); ++i) {
        int key = a[i];
        std::size_t j = i;
        while (j > 0 && a[j - 1] > key) { a[j] = a[j - 1]; --j; }
        a[j] = key;
    }
}

static void selectionSort(std::vector<int>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < n; ++j)
            if (a[j] < a[best]) best = j;
        std::swap(a[i], a[best]);
    }
}

// ---------- merge sort (stable, O(n) scratch) ----------
static void mergeSortRec(std::vector<int>& a, std::vector<int>& buf, std::size_t lo, std::size_t hi) {
    if (hi - lo < 2) return;                       // 0 or 1 element
    std::size_t mid = lo + (hi - lo) / 2;
    mergeSortRec(a, buf, lo, mid);
    mergeSortRec(a, buf, mid, hi);
    std::size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) buf[k++] = (a[j] < a[i]) ? a[j++] : a[i++];   // '<' keeps it stable
    while (i < mid) buf[k++] = a[i++];
    while (j < hi)  buf[k++] = a[j++];
    for (std::size_t x = lo; x < hi; ++x) a[x] = buf[x];
}
static void mergeSort(std::vector<int>& a) {
    std::vector<int> buf(a.size());
    mergeSortRec(a, buf, 0, a.size());
}

// ---------- quicksort (Lomuto partition, median-of-3 pivot) ----------
static void quickSortRec(std::vector<int>& a, std::ptrdiff_t lo, std::ptrdiff_t hi) {
    while (lo < hi) {
        std::ptrdiff_t mid = lo + (hi - lo) / 2;
        const std::size_t L = static_cast<std::size_t>(lo);
        const std::size_t M = static_cast<std::size_t>(mid);
        const std::size_t H = static_cast<std::size_t>(hi);
        // median-of-3: sort a[L],a[M],a[H], then put the MEDIAN at a[H] as the pivot.
        // (Picking a[H] straight after the 3-sort would pick the MAX -> O(n^2) on sorted input.)
        if (a[M] < a[L]) std::swap(a[M], a[L]);
        if (a[H] < a[L]) std::swap(a[H], a[L]);
        if (a[H] < a[M]) std::swap(a[H], a[M]);   // now a[L] <= a[M] <= a[H]
        std::swap(a[M], a[H]);                     // now a[H] = median of the three
        int pivot = a[H];
        std::ptrdiff_t i = lo - 1;
        for (std::ptrdiff_t j = lo; j < hi; ++j)
            if (a[static_cast<std::size_t>(j)] <= pivot)
                std::swap(a[static_cast<std::size_t>(++i)], a[static_cast<std::size_t>(j)]);
        std::swap(a[static_cast<std::size_t>(i + 1)], a[static_cast<std::size_t>(hi)]);
        std::ptrdiff_t p = i + 1;
        // recurse into the smaller half, loop on the larger -> O(log n) stack
        if (p - lo < hi - p) { quickSortRec(a, lo, p - 1); lo = p + 1; }
        else                 { quickSortRec(a, p + 1, hi); hi = p - 1; }
    }
}
static void quickSort(std::vector<int>& a) {
    if (!a.empty()) quickSortRec(a, 0, static_cast<std::ptrdiff_t>(a.size()) - 1);
}

// ---------- heap sort ----------
static void siftDown(std::vector<int>& a, std::size_t start, std::size_t n) {
    std::size_t root = start;
    for (;;) {
        std::size_t child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && a[child] < a[child + 1]) ++child;
        if (a[root] >= a[child]) break;
        std::swap(a[root], a[child]);
        root = child;
    }
}
static void heapSort(std::vector<int>& a) {
    const std::size_t n = a.size();
    if (n < 2) return;
    for (std::size_t i = n / 2; i-- > 0; ) siftDown(a, i, n);      // build max-heap: O(n)
    for (std::size_t end = n - 1; end > 0; --end) {
        std::swap(a[0], a[end]);
        siftDown(a, 0, end);
    }
}

// ---------- harness ----------
template <class F>
static double timeSort(F sortFn, std::vector<int> data) {         // takes a COPY
    auto t0 = Clock::now();
    sortFn(data);
    auto t1 = Clock::now();
    if (!std::is_sorted(data.begin(), data.end())) { std::printf("  !!! NOT SORTED\n"); }
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    std::mt19937 rng{42};

    std::printf("=== correctness (n = 17, random) ===\n");
    {
        std::vector<int> base(17);
        std::uniform_int_distribution<int> d(0, 99);
        for (int& x : base) x = d(rng);
        auto show = [](const char* name, std::vector<int> v, void (*fn)(std::vector<int>&)) {
            fn(v);
            std::printf("  %-10s ", name);
            for (int x : v) std::printf("%d ", x);
            std::printf(" %s\n", std::is_sorted(v.begin(), v.end()) ? "OK" : "FAIL");
        };
        show("bubble",    base, bubbleSort);
        show("insertion", base, insertionSort);
        show("selection", base, selectionSort);
        show("merge",     base, mergeSort);
        show("quick",     base, quickSort);
        show("heap",      base, heapSort);
    }

    std::printf("\n=== benchmark (-O2 for real numbers) ===\n");
    const int SMALL = 20'000;         // O(n^2) sorts choke past this
    const int BIG    = 2'000'000;     // O(n log n) sorts only

    std::vector<int> small(static_cast<std::size_t>(SMALL));
    std::vector<int> big(static_cast<std::size_t>(BIG));
    std::uniform_int_distribution<int> d(0, 1'000'000);
    for (int& x : small) x = d(rng);
    for (int& x : big)   x = d(rng);

    std::printf("\n  n = %d  (O(n^2) sorts):\n", SMALL);
    std::printf("    bubble     : %8.2f ms\n", timeSort(bubbleSort,    small));
    std::printf("    selection  : %8.2f ms\n", timeSort(selectionSort, small));
    std::printf("    insertion  : %8.2f ms\n", timeSort(insertionSort, small));
    std::printf("    std::sort  : %8.2f ms\n", timeSort([](std::vector<int>& v){ std::sort(v.begin(), v.end()); }, small));

    std::printf("\n  n = %d  (O(n log n) sorts):\n", BIG);
    std::printf("    merge      : %8.2f ms\n", timeSort(mergeSort, big));
    std::printf("    quick      : %8.2f ms\n", timeSort(quickSort, big));
    std::printf("    heap       : %8.2f ms\n", timeSort(heapSort,  big));
    std::printf("    std::sort  : %8.2f ms\n", timeSort([](std::vector<int>& v){ std::sort(v.begin(), v.end()); }, big));

    std::printf("\n  n = %d  ALREADY SORTED input (quicksort worst-case danger):\n", BIG);
    std::vector<int> sortedInput = big;
    std::sort(sortedInput.begin(), sortedInput.end());
    std::printf("    quick (med-of-3) : %8.2f ms   (median-of-3 pivot avoids O(n^2))\n",
                timeSort(quickSort, sortedInput));
    std::printf("    std::sort        : %8.2f ms\n",
                timeSort([](std::vector<int>& v){ std::sort(v.begin(), v.end()); }, sortedInput));

    std::printf(
        "\n"
        "  - O(n^2) sorts: fine for n < ~64 (insertion is what std::sort finishes with),\n"
        "    hopeless past a few thousand.\n"
        "  - merge: predictable + stable, pays O(n) scratch + not in place.\n"
        "  - quick: usually fastest hand-written; pivot choice is everything.\n"
        "  - heap: guaranteed O(n log n) in place, but ~2x slower than quick (cache).\n"
        "  - std::sort (introsort) = quick + heapsort fallback + insertion finish. Use it.\n");
    return 0;
}
