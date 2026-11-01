#include "ChangeHistory.h"

#include <utility>

bool ChangeHistory::addStudent(StudentManagement& records, const Student& student) {
    if (!StudentManagement::isValid(student) || records.findStudent(student.rollNumber()))
        return false;
    // Record the inverse before changing the data, so an allocation failure
    // cannot leave an edit with no corresponding undo action.
    entries_.push_back({UndoKind::RemoveAdded, student.rollNumber(), std::nullopt});
    if (records.addStudent(student)) return true;
    entries_.pop_back();
    return false;
}

bool ChangeHistory::updateStudent(StudentManagement& records, int rollNumber,
                                  const Student& replacement) {
    const Student* before = records.findStudent(rollNumber);
    if (!before || replacement.rollNumber() != rollNumber ||
        !StudentManagement::isValid(replacement)) return false;
    entries_.push_back({UndoKind::RestoreUpdated, rollNumber, *before});
    if (records.updateStudent(rollNumber, replacement)) return true;
    entries_.pop_back();
    return false;
}

bool ChangeHistory::deleteStudent(StudentManagement& records, int rollNumber) {
    const Student* before = records.findStudent(rollNumber);
    if (!before) return false;
    entries_.push_back({UndoKind::ReinsertDeleted, rollNumber, *before});
    if (records.deleteStudent(rollNumber)) return true;
    entries_.pop_back();
    return false;
}

bool ChangeHistory::undo(StudentManagement& records) {
    if (entries_.empty()) return false;
    const UndoEntry& entry = entries_.back();
    bool restored = false;
    switch (entry.kind) {
        case UndoKind::RemoveAdded:
            restored = records.deleteStudent(entry.rollNumber);
            break;
        case UndoKind::RestoreUpdated:
            restored = entry.original &&
                       records.updateStudent(entry.rollNumber, *entry.original);
            break;
        case UndoKind::ReinsertDeleted:
            restored = entry.original && records.addStudent(*entry.original);
            break;
    }
    if (restored) entries_.pop_back();
    return restored;
}

std::size_t ChangeHistory::undoCount() const { return entries_.size(); }
void ChangeHistory::clear() { entries_.clear(); }
