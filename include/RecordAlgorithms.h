#ifndef RECORD_ALGORITHMS_H
#define RECORD_ALGORITHMS_H

#include "Student.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace RecordAlgorithms {

// These views point to the original records. Do not keep them after editing
// the student collection.
using StudentView = std::vector<const Student*>;

struct RankedStudent {
    const Student* student;
    std::size_t rank;
};

const Student* linearSearch(const std::vector<Student>& students, int rollNumber);
const Student* binarySearch(const std::vector<Student>& students, int rollNumber);

StudentView sortByRollNumber(const std::vector<Student>& students);
StudentView sortByPercentage(const std::vector<Student>& students);
StudentView sortByTotalMarks(const std::vector<Student>& students);

int totalMarks(const Student& student);
double percentage(const Student& student);

std::vector<RankedStudent> rankList(const std::vector<Student>& students);
std::optional<std::size_t> rankOf(const std::vector<Student>& students,
                                  int rollNumber);

}  // namespace RecordAlgorithms

#endif
