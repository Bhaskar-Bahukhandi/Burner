#include "RecordAlgorithms.h"

#include <numeric>

namespace {
using RecordAlgorithms::StudentView;

StudentView makeView(const std::vector<Student>& students) {
    StudentView view;
    view.reserve(students.size());
    for (const Student& student : students) view.push_back(&student);
    return view;
}

template <typename ComesFirst>
void insertionSort(StudentView& view, ComesFirst comesFirst) {
    for (std::size_t i = 1; i < view.size(); ++i) {
        const Student* current = view[i];
        std::size_t j = i;
        while (j > 0 && comesFirst(*current, *view[j - 1])) {
            view[j] = view[j - 1];
            --j;
        }
        view[j] = current;
    }
}

// Exact comparison, avoiding rounding when students have different
// numbers of subjects. Marks are out of 100 in this project.
bool higherPercentage(const Student& a, const Student& b) {
    const int aTotal = RecordAlgorithms::totalMarks(a);
    const int bTotal = RecordAlgorithms::totalMarks(b);
    const auto aCount = static_cast<int>(a.marks().size());
    const auto bCount = static_cast<int>(b.marks().size());
    const int aScaled = aTotal * bCount;
    const int bScaled = bTotal * aCount;
    if (aScaled != bScaled) return aScaled > bScaled;
    if (aTotal != bTotal) return aTotal > bTotal;
    return a.rollNumber() < b.rollNumber();
}
}  // namespace

namespace RecordAlgorithms {

int totalMarks(const Student& student) {
    return std::accumulate(student.marks().begin(), student.marks().end(), 0);
}

double percentage(const Student& student) {
    if (student.marks().empty()) return 0.0;
    return static_cast<double>(totalMarks(student)) / student.marks().size();
}

const Student* linearSearch(const std::vector<Student>& students, int rollNumber) {
    for (const Student& student : students) {
        if (student.rollNumber() == rollNumber) return &student;
    }
    return nullptr;
}

StudentView sortByRollNumber(const std::vector<Student>& students) {
    StudentView view = makeView(students);
    insertionSort(view, [](const Student& a, const Student& b) {
        return a.rollNumber() < b.rollNumber();
    });
    return view;
}

const Student* binarySearch(const std::vector<Student>& students, int rollNumber) {
    // Binary search requires ordered data. Prepare a sorted view first.
    StudentView sorted = sortByRollNumber(students);
    std::size_t low = 0;
    std::size_t high = sorted.size();

    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (sorted[mid]->rollNumber() == rollNumber) return sorted[mid];
        if (sorted[mid]->rollNumber() < rollNumber) low = mid + 1;
        else high = mid;
    }
    return nullptr;
}

StudentView sortByPercentage(const std::vector<Student>& students) {
    StudentView view = makeView(students);
    insertionSort(view, higherPercentage);
    return view;
}

StudentView sortByTotalMarks(const std::vector<Student>& students) {
    StudentView view = makeView(students);
    insertionSort(view, [](const Student& a, const Student& b) {
        const int aTotal = totalMarks(a);
        const int bTotal = totalMarks(b);
        if (aTotal != bTotal) return aTotal > bTotal;
        return higherPercentage(a, b);
    });
    return view;
}

std::vector<RankedStudent> rankList(const std::vector<Student>& students) {
    StudentView sorted = sortByPercentage(students);
    std::vector<RankedStudent> result;
    result.reserve(sorted.size());
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        result.push_back({sorted[i], i + 1});
    }
    return result;
}

std::optional<std::size_t> rankOf(const std::vector<Student>& students,
                                  int rollNumber) {
    for (const RankedStudent& item : rankList(students)) {
        if (item.student->rollNumber() == rollNumber) return item.rank;
    }
    return std::nullopt;
}

}  // namespace RecordAlgorithms
