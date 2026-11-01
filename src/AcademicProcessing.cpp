#include "AcademicProcessing.h"
#include "RecordAlgorithms.h"

#include <algorithm>

namespace AcademicProcessing {

GradeInfo gradeInfo(double percentage, GradeScale scale) {
    if (scale == GradeScale::OriginalProject) {
        if (percentage >= 90.0) return {"A", "Project scale", -1};
        if (percentage >= 80.0) return {"B", "Project scale", -1};
        if (percentage >= 70.0) return {"C", "Project scale", -1};
        if (percentage >= 60.0) return {"D", "Project scale", -1};
        return {"F", "Project scale", -1};
    }

    // Reference: GEU distance-learning PPR. The published table uses >85
    // for O and 75-84 for A+, leaving 85 unstated. We keep 85 in A+ here
    // rather than claiming an unverified O cutoff for that one boundary.
    if (percentage > 85.0) return {"O", "Outstanding", 10};
    if (percentage >= 75.0) return {"A+", "Excellent", 9};
    if (percentage >= 65.0) return {"A", "Very Good", 8};
    if (percentage >= 55.0) return {"B+", "Good", 7};
    if (percentage >= 50.0) return {"B", "Above Average", 6};
    if (percentage >= 45.0) return {"C", "Average", 5};
    if (percentage >= 40.0) return {"P", "Pass", 4};
    return {"F", "Fail", 0};
}

std::string grade(double percentage, GradeScale scale) {
    return gradeInfo(percentage, scale).letter;
}

std::string scaleName(GradeScale scale) {
    return scale == GradeScale::GeuReference
        ? "GEU published reference (illustrative, not verified for B.Tech)"
        : "Original EduTrack project scale";
}

std::string category(double percentage, GradeScale scale) {
    if (scale == GradeScale::GeuReference) {
        // Four project bands grouped around the published reference grades.
        // These bands are NOT university-defined grading categories.
        if (percentage >= 75.0) return "Excellent";
        if (percentage >= 55.0) return "Good";
        if (percentage >= 45.0) return "Average";
        return "Needs Improvement";
    }
    if (percentage >= 85.0) return "Excellent";
    if (percentage >= 70.0) return "Good";
    if (percentage >= 50.0) return "Average";
    return "Needs Improvement";
}

std::optional<double> classAverage(const std::vector<Student>& students) {
    if (students.empty()) return std::nullopt;

    double sum = 0.0;
    for (const Student& student : students) {
        sum += RecordAlgorithms::percentage(student);
    }
    return sum / static_cast<double>(students.size());
}

std::optional<std::vector<SubjectResult>> groupSubjectResults(
    const StudentManagement& records, const AcademicGroups& groups,
    const std::string& groupId) {
    const AcademicGroup* group = groups.findGroup(groupId);
    if (!group || !groups.consistentWith(records)) return std::nullopt;

    std::vector<SubjectResult> result;
    result.reserve(group->subjects.size());
    for (const SubjectDefinition& subject : group->subjects)
        result.push_back({subject.code, subject.name, 0, 0.0, 100, 0});

    std::size_t members = 0;
    for (const Student& student : records.allStudents()) {
        const AcademicGroup* assigned = groups.groupFor(student);
        if (!assigned || assigned->id != groupId) continue;
        if (student.marks().size() != result.size()) return std::nullopt;
        ++members;
        for (std::size_t index = 0; index < result.size(); ++index) {
            SubjectResult& subject = result[index];
            const int mark = student.marks()[index];
            subject.average += mark;
            ++subject.students;
            subject.lowest = std::min(subject.lowest, mark);
            subject.highest = std::max(subject.highest, mark);
        }
    }
    if (members == 0) return std::vector<SubjectResult>{};
    for (SubjectResult& subject : result)
        subject.average /= static_cast<double>(subject.students);
    return result;
}

const Student* topStudent(const std::vector<Student>& students) {
    auto ranked = RecordAlgorithms::sortByPercentage(students);
    return ranked.empty() ? nullptr : ranked.front();
}

}  // namespace AcademicProcessing
