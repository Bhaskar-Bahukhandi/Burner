#include "ImprovementGoalPlanner.h"
#include "RecordAlgorithms.h"

#include <numeric>

namespace {
using ImprovementGoalPlanner::Result;
using ImprovementGoalPlanner::Status;
using ImprovementGoalPlanner::SubjectPlan;
using AcademicProcessing::GradeScale;

std::optional<Result> compute(const Student& student, int requiredTotal,
                              GradeScale scale, const std::vector<int>& ceilings) {
    const std::vector<int>& marks = student.marks();
    if (marks.empty() || marks.size() > 20 ||
        (!ceilings.empty() && ceilings.size() != marks.size()) ||
        requiredTotal < 0 || requiredTotal > static_cast<int>(100 * marks.size())) {
        return std::nullopt;
    }
    std::vector<SubjectPlan> subjects;
    subjects.reserve(marks.size());
    int maximumTotal = 0;
    for (std::size_t i = 0; i < marks.size(); ++i) {
        const int current = marks[i];
        const int maximum = ceilings.empty() ? 100 : ceilings[i];
        if (current < 0 || current > 100 || maximum < current || maximum > 100) {
            return std::nullopt;
        }
        subjects.push_back({i + 1, current, maximum, current});
        maximumTotal += maximum;
    }
    const int currentTotal = RecordAlgorithms::totalMarks(student);
    const Status status = requiredTotal <= currentTotal ? Status::AlreadyMet :
                          requiredTotal > maximumTotal ? Status::Unreachable :
                          Status::Feasible;
    const int desired = status == Status::AlreadyMet ? currentTotal :
                        status == Status::Unreachable ? maximumTotal : requiredTotal;
    int remaining = desired - currentTotal;

    // Transparent, deterministic balancing rule: increase the currently lowest
    // projected mark with remaining capacity, breaking ties by subject order.
    // This is NOT an effort estimate or a prediction about learning difficulty.
    while (remaining > 0) {
        std::size_t best = subjects.size();
        for (std::size_t i = 0; i < subjects.size(); ++i) {
            if (subjects[i].proposedMark >= subjects[i].maximumMark) continue;
            if (best == subjects.size() ||
                subjects[i].proposedMark < subjects[best].proposedMark) best = i;
        }
        if (best == subjects.size()) return std::nullopt; // defensive guard
        ++subjects[best].proposedMark;
        --remaining;
    }
    const double divisor = static_cast<double>(marks.size());
    return Result{status, currentTotal, requiredTotal, maximumTotal, desired,
                  currentTotal / divisor, desired / divisor,
                  AcademicProcessing::gradeInfo(currentTotal / divisor, scale),
                  AcademicProcessing::gradeInfo(desired / divisor, scale),
                  std::move(subjects)};
}

std::optional<int> gradeThreshold(const std::string& letter, GradeScale scale,
                                  int subjectCount) {
    if (scale == GradeScale::OriginalProject) {
        if (letter == "A") return 90 * subjectCount;
        if (letter == "B") return 80 * subjectCount;
        if (letter == "C") return 70 * subjectCount;
        if (letter == "D") return 60 * subjectCount;
        return std::nullopt;
    }
    if (letter == "O") return 85 * subjectCount + 1; // strictly >85%
    if (letter == "A+") return 75 * subjectCount;
    if (letter == "A") return 65 * subjectCount;
    if (letter == "B+") return 55 * subjectCount;
    if (letter == "B") return 50 * subjectCount;
    if (letter == "C") return 45 * subjectCount;
    if (letter == "P") return 40 * subjectCount;
    return std::nullopt;
}
} // namespace

namespace ImprovementGoalPlanner {

std::optional<Result> forPercentage(const Student& student, int targetHundredths,
                                    GradeScale scale, const std::vector<int>& ceilings) {
    if (targetHundredths < 0 || targetHundredths > 10000) return std::nullopt;
    const int count = static_cast<int>(student.marks().size());
    if (count < 1 || count > 20) return std::nullopt;
    // Each total-mark increment changes the percentage by 1/count points.
    // Required total = ceil(target percentage * count), with integer arithmetic.
    const int required = (targetHundredths * count + 99) / 100;
    return compute(student, required, scale, ceilings);
}

std::optional<Result> forGrade(const Student& student, const std::string& grade,
                               GradeScale scale, const std::vector<int>& ceilings) {
    const int count = static_cast<int>(student.marks().size());
    if (count < 1 || count > 20) return std::nullopt;
    const auto required = gradeThreshold(grade, scale, count);
    return required ? compute(student, *required, scale, ceilings) : std::nullopt;
}

} // namespace ImprovementGoalPlanner
