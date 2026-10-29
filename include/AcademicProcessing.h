#ifndef ACADEMIC_PROCESSING_H
#define ACADEMIC_PROCESSING_H

#include "Student.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Positional statistic: valid only when all records share the SAME subject order.
// Prefer groupedSubjectResults for class reports.
struct SubjectResult {
    std::size_t number;
    std::size_t students;
    double average;
    int lowest;
    int highest;
};

class AcademicGroups;

// A subject identity is the group ID + stable subject code. Legacy positional
// marks are never silently interpreted across unrelated groups.
struct GroupSubjectResult {
    std::string groupId;
    std::string subjectCode;
    std::string subjectName;
    std::size_t position;
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
std::vector<SubjectResult> subjectResults(const std::vector<Student>& students);
std::vector<GroupSubjectResult> groupedSubjectResults(
    const std::vector<Student>& students, const AcademicGroups& groups);
std::size_t unassignedSubjectRecords(
    const std::vector<Student>& students, const AcademicGroups& groups);
const Student* topStudent(const std::vector<Student>& students);

}  // namespace AcademicProcessing

#endif
