#include "StudentManagement.h"

#include <algorithm>
#include <cctype>

namespace {
bool hasText(const std::string& value) {
    if (std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return std::iscntrl(c);
    })) return false;
    return std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return !std::isspace(c);
    });
}
}  // namespace

bool StudentManagement::isValid(const Student& student) {
    if (student.rollNumber() <= 0 || student.rollNumber() > 999999999 ||
        student.name().size() > 80 ||
        student.branch().size() > 60 || !hasText(student.name()) ||
        !hasText(student.branch()) || student.semester() < 1 ||
        student.semester() > 12 || student.marks().empty() ||
        student.marks().size() > 20) {
        return false;
    }

    for (int mark : student.marks()) {
        if (mark < 0 || mark > 100) return false;
    }
    return true;
}

const Student* StudentManagement::findStudent(int rollNumber) const {
    const auto position = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& student, int roll) { return student.rollNumber() < roll; });
    return position != students_.end() && position->rollNumber() == rollNumber
        ? &*position : nullptr;
}

bool StudentManagement::addStudent(const Student& student) {
    if (!isValid(student)) return false;
    auto position = std::lower_bound(students_.begin(), students_.end(), student.rollNumber(),
        [](const Student& existing, int roll) { return existing.rollNumber() < roll; });
    if (position != students_.end() && position->rollNumber() == student.rollNumber())
        return false;
    students_.insert(position, student); // O(n) insertion; lookups remain O(log n).
    return true;
}

bool StudentManagement::updateStudent(int rollNumber, const Student& replacement) {
    if (rollNumber != replacement.rollNumber() || !isValid(replacement)) return false;
    auto position = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& existing, int roll) { return existing.rollNumber() < roll; });
    if (position == students_.end() || position->rollNumber() != rollNumber) return false;
    *position = replacement; // Roll number stays the same; order remains sorted.
    return true;
}

bool StudentManagement::deleteStudent(int rollNumber) {
    auto position = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& existing, int roll) { return existing.rollNumber() < roll; });
    if (position == students_.end() || position->rollNumber() != rollNumber) return false;
    students_.erase(position);
    return true;
}

const std::vector<Student>& StudentManagement::allStudents() const {
    return students_;
}
