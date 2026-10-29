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

// Keep the collection ordered by roll number. Searches are logarithmic;
// insertion/deletion shift vector elements (O(n)), and preserve the invariant
// across copies, undo restores, and SQLite snapshot loads.
const Student* StudentManagement::findStudent(int rollNumber) const {
    auto it = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& student, int key) { return student.rollNumber() < key; });
    return it != students_.end() && it->rollNumber() == rollNumber ? &*it : nullptr;
}

bool StudentManagement::addStudent(const Student& student) {
    if (!isValid(student)) return false;
    auto pos = std::lower_bound(students_.begin(), students_.end(), student.rollNumber(),
        [](const Student& item, int key) { return item.rollNumber() < key; });
    if (pos != students_.end() && pos->rollNumber() == student.rollNumber()) return false;
    students_.insert(pos, student);
    return true;
}

bool StudentManagement::updateStudent(int rollNumber, const Student& replacement) {
    if (rollNumber != replacement.rollNumber() || !isValid(replacement)) return false;
    auto pos = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& item, int key) { return item.rollNumber() < key; });
    if (pos == students_.end() || pos->rollNumber() != rollNumber) return false;
    *pos = replacement;
    return true;
}

bool StudentManagement::deleteStudent(int rollNumber) {
    auto pos = std::lower_bound(students_.begin(), students_.end(), rollNumber,
        [](const Student& item, int key) { return item.rollNumber() < key; });
    if (pos == students_.end() || pos->rollNumber() != rollNumber) return false;
    students_.erase(pos);
    return true;
}

const std::vector<Student>& StudentManagement::allStudents() const {
    return students_;
}
