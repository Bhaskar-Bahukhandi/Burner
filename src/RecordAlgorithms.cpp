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

// Merge sort keeps classroom-sized datasets fast even when they grow.
// A separate buffer avoids copying the Student records themselves.
template <typename ComesFirst>
void mergeRange(StudentView& view, StudentView& buffer,
                std::size_t left, std::size_t right, ComesFirst comesFirst) {
    if (right - left < 2) return;
    const std::size_t middle = left + (right - left) / 2;
    mergeRange(view, buffer, left, middle, comesFirst);
    mergeRange(view, buffer, middle, right, comesFirst);

    std::size_t i = left, j = middle, k = left;
    while (i < middle && j < right) {
        // Take the left element when equal, making the sort stable.
        if (comesFirst(*view[j], *view[i])) buffer[k++] = view[j++];
        else buffer[k++] = view[i++];
    }
    while (i < middle) buffer[k++] = view[i++];
    while (j < right) buffer[k++] = view[j++];
    for (std::size_t n = left; n < right; ++n) view[n] = buffer[n];
}

template <typename ComesFirst>
void mergeSort(StudentView& view, ComesFirst comesFirst) {
    StudentView buffer(view.size());
    mergeRange(view, buffer, 0, view.size(), comesFirst);
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
    mergeSort(view, [](const Student& a, const Student& b) {
        return a.rollNumber() < b.rollNumber();
    });
    return view;
}

const Student* binarySearch(const std::vector<Student>& students, int rollNumber) {
    // The records are already sorted by StudentManagement; no re-sort here.
    std::size_t low = 0, high = students.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (students[mid].rollNumber() == rollNumber) return &students[mid];
        if (students[mid].rollNumber() < rollNumber) low = mid + 1;
        else high = mid;
    }
    return nullptr;
}

StudentView sortByPercentage(const std::vector<Student>& students) {
    StudentView view = makeView(students);
    mergeSort(view, higherPercentage);
    return view;
}

StudentView sortByTotalMarks(const std::vector<Student>& students) {
    StudentView view = makeView(students);
    mergeSort(view, [](const Student& a, const Student& b) {
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
    std::size_t rank = 0;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        if (i == 0 || totalMarks(*sorted[i]) * static_cast<int>(sorted[i - 1]->marks().size()) !=
            totalMarks(*sorted[i - 1]) * static_cast<int>(sorted[i]->marks().size())) {
            rank = i + 1;
        }
        result.push_back({sorted[i], rank});
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
