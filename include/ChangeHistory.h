#ifndef CHANGE_HISTORY_H
#define CHANGE_HISTORY_H

#include "StudentManagement.h"

#include <cstddef>
#include <optional>
#include <vector>

// Stores one inverse action per successful change, not a whole class snapshot.
// History is session-only. The database still handles permanent records.
class ChangeHistory {
public:
    bool addStudent(StudentManagement& records, const Student& student);
    bool updateStudent(StudentManagement& records, int rollNumber,
                       const Student& replacement);
    bool deleteStudent(StudentManagement& records, int rollNumber);

    bool undo(StudentManagement& records);
    std::size_t undoCount() const;
    void clear();

private:
    enum class UndoKind { RemoveAdded, RestoreUpdated, ReinsertDeleted };
    struct UndoEntry {
        UndoKind kind;
        int rollNumber;
        std::optional<Student> original;
    };
    std::vector<UndoEntry> entries_;
};

#endif
