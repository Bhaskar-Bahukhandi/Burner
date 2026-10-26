#ifndef IMPROVEMENT_SIMULATOR_H
#define IMPROVEMENT_SIMULATOR_H

#include "AcademicProcessing.h"
#include "Student.h"

#include <cstddef>
#include <optional>

namespace ImprovementSimulator {

struct Result {
    std::size_t subjectNumber; // 1-based number for display
    int originalMark;
    int proposedMark;
    int originalTotal;
    int projectedTotal;
    double originalPercentage;
    double projectedPercentage;
    AcademicProcessing::GradeInfo originalGrade;
    AcademicProcessing::GradeInfo projectedGrade;
};

// Returns a hypothetical result. The Student object and saved data are unchanged.
// subjectNumber starts from 1, matching the subject labels in EduTrack.
// Invalid subject numbers, marks and malformed source marks return no result.
std::optional<Result> simulate(const Student& student, std::size_t subjectNumber,
                               int proposedMark,
                               AcademicProcessing::GradeScale scale =
                                   AcademicProcessing::GradeScale::GeuReference);

}  // namespace ImprovementSimulator

#endif
