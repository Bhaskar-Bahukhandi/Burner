#include "ChangeHistory.h"

#include <utility>

bool ChangeHistory::addStudent(StudentManagement& records, const Student& student) {
    StudentManagement changed = records;
    if (!changed.addStudent(student)) return false;

    previousStates_.push(records);
    records = std::move(changed);
    return true;
}

bool ChangeHistory::updateStudent(StudentManagement& records, int rollNumber,
                                  const Student& replacement) {
    StudentManagement changed = records;
    if (!changed.updateStudent(rollNumber, replacement)) return false;

    previousStates_.push(records);
    records = std::move(changed);
    return true;
}

bool ChangeHistory::deleteStudent(StudentManagement& records, int rollNumber) {
    StudentManagement changed = records;
    if (!changed.deleteStudent(rollNumber)) return false;

    previousStates_.push(records);
    records = std::move(changed);
    return true;
}

bool ChangeHistory::undo(StudentManagement& records) {
    if (previousStates_.empty()) return false;

    StudentManagement restored = previousStates_.top();
    records = std::move(restored);
    previousStates_.pop();
    return true;
}

std::size_t ChangeHistory::undoCount() const {
    return previousStates_.size();
}

void ChangeHistory::clear() {
    std::stack<StudentManagement> empty;
    previousStates_.swap(empty);
}
