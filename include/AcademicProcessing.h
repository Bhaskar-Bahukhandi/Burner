#ifndef ACADEMIC_PROCESSING_H
#define ACADEMIC_PROCESSING_H

#include "Student.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct SubjectResult {
    std::size_t number;
    std::size_t students;
    double average;
    int lowest;
    int highest;
};

namespace AcademicProcessing {

std::string grade(double percentage);
std::string category(double percentage);
std::optional<double> classAverage(const std::vector<Student>& students);
std::vector<SubjectResult> subjectResults(const std::vector<Student>& students);
const Student* topStudent(const std::vector<Student>& students);

}  // namespace AcademicProcessing

#endif
