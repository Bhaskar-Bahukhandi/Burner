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
    const std::vector<Student>& allStudents() const;

private:
    std::vector<Student> students_;
    static bool isValid(const Student& student);
};

#endif
