#include "ImprovementSimulator.h"
#include "RecordAlgorithms.h"

namespace ImprovementSimulator {

std::optional<Result> simulate(const Student& student, std::size_t subjectNumber,
                               int proposedMark, AcademicProcessing::GradeScale scale) {
    const auto& marks = student.marks();
    if (marks.empty() || marks.size() > 20 || subjectNumber == 0 ||
        subjectNumber > marks.size() || proposedMark < 0 || proposedMark > 100) {
        return std::nullopt;
    }
    for (int mark : marks) {
        if (mark < 0 || mark > 100) return std::nullopt;
    }

    const int oldTotal = RecordAlgorithms::totalMarks(student);
    const int oldMark = marks[subjectNumber - 1];
    const int newTotal = oldTotal - oldMark + proposedMark;
    const double subjectCount = static_cast<double>(marks.size());
    const double oldPercentage = static_cast<double>(oldTotal) / subjectCount;
    const double newPercentage = static_cast<double>(newTotal) / subjectCount;

    return Result{
        subjectNumber,
        oldMark,
        proposedMark,
        oldTotal,
        newTotal,
        oldPercentage,
        newPercentage,
        AcademicProcessing::gradeInfo(oldPercentage, scale),
        AcademicProcessing::gradeInfo(newPercentage, scale)
    };
}

}  // namespace ImprovementSimulator
