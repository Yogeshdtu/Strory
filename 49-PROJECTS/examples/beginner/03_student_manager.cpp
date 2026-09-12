// 03_student_manager.cpp  --  49-PROJECTS beginner P3
// ============================================================
// In-memory student records: add / remove / find / averages / topper /
// subject-wise averages. struct + vector + <algorithm> + <numeric>.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 03_student_manager.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kSubjects = 3;

struct Student {
    int                              id{};
    std::string                      name;
    std::array<int, kSubjects>       marks{};
};

double average(const Student& s) {
    const int total = std::accumulate(s.marks.begin(), s.marks.end(), 0);
    return static_cast<double>(total) / static_cast<double>(kSubjects);
}

class Roster {
public:
    bool add(int id, std::string name, std::array<int, kSubjects> marks) {
        if (find(id) != nullptr) return false;                 // ids are unique
        for (int m : marks) if (m < 0 || m > 100) return false; // invariant
        students_.push_back({id, std::move(name), marks});
        return true;
    }
    bool remove(int id) {
        const auto before = students_.size();
        std::erase_if(students_, [id](const Student& s) { return s.id == id; });
        return students_.size() != before;
    }
    const Student* find(int id) const {
        for (const auto& s : students_) if (s.id == id) return &s;
        return nullptr;
    }
    std::size_t size() const { return students_.size(); }

    std::vector<Student> by_average_desc() const {
        std::vector<Student> v = students_;
        std::stable_sort(v.begin(), v.end(),
                         [](const Student& a, const Student& b) { return average(a) > average(b); });
        return v;
    }
    std::optional<Student> topper() const {
        if (students_.empty()) return std::nullopt;
        return *std::max_element(students_.begin(), students_.end(),
                                 [](const Student& a, const Student& b) { return average(a) < average(b); });
    }
    std::array<double, kSubjects> subject_averages() const {
        std::array<long long, kSubjects> sum{};
        for (const auto& s : students_)
            for (std::size_t j = 0; j < kSubjects; ++j) sum[j] += s.marks[j];
        std::array<double, kSubjects> avg{};
        const auto n = static_cast<double>(students_.size() ? students_.size() : 1);
        for (std::size_t j = 0; j < kSubjects; ++j) avg[j] = static_cast<double>(sum[j]) / n;
        return avg;
    }

private:
    std::vector<Student> students_;
};

bool close(double a, double b) { return std::fabs(a - b) < 1e-9; }

} // namespace

int main() {
    Roster r;
    assert(r.add(1, "Asha",  {90, 80, 70}));
    assert(r.add(2, "Bilal", {60, 60, 60}));
    assert(r.add(3, "Chetna",{100, 95, 99}));
    assert(r.size() == 3);

    assert(!r.add(1, "Dup", {50, 50, 50}));         // duplicate id rejected
    assert(!r.add(4, "Bad", {50, 150, 50}));        // out-of-range mark rejected
    assert(r.size() == 3);

    assert(r.find(2) != nullptr && r.find(2)->name == "Bilal");
    assert(r.find(99) == nullptr);

    assert(close(average(*r.find(1)), 240.0 / 3.0));

    const auto ranked = r.by_average_desc();
    assert(ranked.size() == 3);
    assert(ranked[0].id == 3 && ranked[1].id == 1 && ranked[2].id == 2);

    assert(r.topper()->id == 3);

    const auto sa = r.subject_averages();
    assert(close(sa[0], (90 + 60 + 100) / 3.0));
    assert(close(sa[1], (80 + 60 + 95)  / 3.0));
    assert(close(sa[2], (70 + 60 + 99)  / 3.0));

    assert(r.remove(2));
    assert(!r.remove(2));                            // already gone
    assert(r.size() == 2 && r.find(2) == nullptr);

    // empty roster edge cases
    Roster empty;
    assert(empty.topper() == std::nullopt);
    assert(empty.by_average_desc().empty());

    std::puts("03_student_manager: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - struct Student is a plain aggregate; Roster owns the vector and guards
//     the invariants (unique id, marks in [0,100]).
//   - std::erase_if (C++20) is the one-call erase-remove. by_average_desc uses
//     stable_sort so equal averages keep insertion order.
//   - subject_averages sums into long long (can't overflow) then converts once.
//   - Concepts: aggregate init (11), <algorithm>/<numeric> (19), std::optional
//     for "maybe no topper" (23).
// ============================================================
