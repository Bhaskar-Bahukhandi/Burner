#ifndef IMPROVEMENT_GOAL_PLANNER_H
#define IMPROVEMENT_GOAL_PLANNER_H

#include "AcademicProcessing.h"
#include "Student.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ImprovementGoalPlanner {

enum class Status { AlreadyMet, Feasible, Unreachable };

struct SubjectPlan {
    std::size_t number; // one-based, same order as stored marks
    int currentMark;
    int maximumMark;
    int proposedMark;
};

struct Result {
    Status status;
    int currentTotal;
    int requiredTotal; // smallest whole-number total satisfying the selected goal
    int maximumTotal;  // attainable with the user-entered subject ceilings
    int proposedTotal;
    double currentPercentage;
    double proposedPercentage;
    AcademicProcessing::GradeInfo currentGrade;
    AcademicProcessing::GradeInfo proposedGrade;
    std::vector<SubjectPlan> subjects;
};

// Percentage in hundredths: 7525 means 75.25%. The requested target is
// met using exact integer comparisons, never a rounded displayed percentage.
// An empty ceilings vector means 100 in every subject. Invalid input => nullopt.
std::optional<Result> forPercentage(const Student& student, int targetHundredths,
                                    AcademicProcessing::GradeScale scale,
                                    const std::vector<int>& ceilings = {});

// Grade goals use the current *illustrative* grading scale, not an official
// university policy. F is not an improvement goal. Invalid goal => nullopt.
std::optional<Result> forGrade(const Student& student, const std::string& grade,
                               AcademicProcessing::GradeScale scale,
                               const std::vector<int>& ceilings = {});

} // namespace ImprovementGoalPlanner

#endif
