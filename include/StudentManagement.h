#ifndef STUDENT_MANAGEMENT_H
#define STUDENT_MANAGEMENT_H

#include "Student.h"

#include <vector>

class StudentManagement {
public:
    bool addStudent(const Student& student);
    bool updateStudent(int rollNumber, const Student& replacement);
    bool deleteStudent(int rollNumber);
    const Student* findStudent(int rollNumber) const;
    // Always sorted by roll. Finding a student is O(log n).
    const std::vector<Student>& allStudents() const;
    // Shared validation for the console, desktop form and record operations.
    static bool isValid(const Student& student);

private:
    std::vector<Student> students_;
};

#endif
