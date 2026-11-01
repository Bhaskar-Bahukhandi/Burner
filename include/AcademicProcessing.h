#ifndef ACADEMIC_PROCESSING_H
#define ACADEMIC_PROCESSING_H

#include "Student.h"
#include "AcademicGroups.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct SubjectResult {
    std::string code;
    std::string name;
    std::size_t students;
    double average;
    int lowest;
    int highest;
};

namespace AcademicProcessing {

// The reference table comes from GEU's distance-learning documents.
// It is not a confirmed B.Tech grading policy.
enum class GradeScale { GeuReference, OriginalProject };

struct GradeInfo {
    std::string letter;
    std::string rating;
    int points; // -1 means this scheme has no defined grade points
};

GradeInfo gradeInfo(double percentage, GradeScale scale = GradeScale::GeuReference);
std::string grade(double percentage, GradeScale scale = GradeScale::GeuReference);
std::string scaleName(GradeScale scale);
std::string category(double percentage, GradeScale scale = GradeScale::GeuReference);
std::optional<double> classAverage(const std::vector<Student>& students);
// Only compare marks within the same explicitly assigned academic group.
// Missing/invalid group or inconsistent assignments => nullopt.
std::optional<std::vector<SubjectResult>> groupSubjectResults(
    const StudentManagement& records, const AcademicGroups& groups,
    const std::string& groupId);
const Student* topStudent(const std::vector<Student>& students);

}  // namespace AcademicProcessing

#endif
