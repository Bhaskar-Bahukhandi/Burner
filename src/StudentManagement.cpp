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
    for (const Student& student : students_) {
        if (student.rollNumber() == rollNumber) return &student;
    }
    return nullptr;
}

bool StudentManagement::addStudent(const Student& student) {
    if (!isValid(student) || findStudent(student.rollNumber())) return false;
    students_.push_back(student);
    return true;
}

bool StudentManagement::updateStudent(int rollNumber, const Student& replacement) {
    if (rollNumber != replacement.rollNumber() || !isValid(replacement)) return false;

    for (Student& student : students_) {
        if (student.rollNumber() == rollNumber) {
            student = replacement;
            return true;
        }
    }
    return false;
}

bool StudentManagement::deleteStudent(int rollNumber) {
    for (auto it = students_.begin(); it != students_.end(); ++it) {
        if (it->rollNumber() == rollNumber) {
            students_.erase(it);
            return true;
        }
    }
    return false;
}

const std::vector<Student>& StudentManagement::allStudents() const {
    return students_;
}
