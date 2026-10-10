#ifndef CHANGE_HISTORY_H
#define CHANGE_HISTORY_H

#include "StudentManagement.h"

#include <cstddef>
#include <stack>

// Keeps the state before each successful change so it can be restored.
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
    std::stack<StudentManagement> previousStates_;
};

#endif
