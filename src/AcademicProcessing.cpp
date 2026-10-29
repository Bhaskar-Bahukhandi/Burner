#include "AcademicProcessing.h"
#include "RecordAlgorithms.h"
#include "AcademicGroups.h"

#include <algorithm>
#include <map>

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

std::vector<SubjectResult> subjectResults(const std::vector<Student>& students) {
    std::vector<SubjectResult> result;
    for (const Student& student : students) {
        const auto& marks = student.marks();
        for (std::size_t i = 0; i < marks.size(); ++i) {
            if (i == result.size()) result.push_back({i + 1, 0, 0.0, 100, 0});
            SubjectResult& subject = result[i];
            subject.average += marks[i]; // Sum first, divide once after all records.
            ++subject.students;
            subject.lowest = std::min(subject.lowest, marks[i]);
            subject.highest = std::max(subject.highest, marks[i]);
        }
    }
    for (SubjectResult& subject : result) {
        subject.average /= static_cast<double>(subject.students);
    }
    return result;
}

const Student* topStudent(const std::vector<Student>& students) {
    auto ranked = RecordAlgorithms::sortByPercentage(students);
    return ranked.empty() ? nullptr : ranked.front();
}


std::vector<GroupSubjectResult> groupedSubjectResults(
    const std::vector<Student>& students, const AcademicGroups& groups) {
    // Position is meaningful only within a validated academic group's
    // subject definition. Never aggregate position 1 across unrelated groups.
    std::map<std::string, std::vector<GroupSubjectResult>> byGroup;
    for (const Student& student : students) {
        const AcademicGroup* group = groups.groupFor(student);
        if (!group) continue; // Unassigned records have no verifiable subject ID.
        auto& subjects = byGroup[group->id];
        if (subjects.empty()) {
            for (std::size_t i = 0; i < group->subjects.size(); ++i) {
                const auto& subject = group->subjects[i];
                subjects.push_back({group->id, subject.code, subject.name, i + 1,
                                    0, 0.0, 100, 0});
            }
        }
        for (std::size_t i = 0; i < subjects.size(); ++i) {
            GroupSubjectResult& result = subjects[i];
            const int mark = student.marks()[i];
            result.average += mark;
            ++result.students;
            result.lowest = std::min(result.lowest, mark);
            result.highest = std::max(result.highest, mark);
        }
    }
    std::vector<GroupSubjectResult> result;
    for (auto& entry : byGroup) {
        for (auto& subject : entry.second) {
            subject.average /= static_cast<double>(subject.students);
            result.push_back(subject);
        }
    }
    return result;
}

std::size_t unassignedSubjectRecords(
    const std::vector<Student>& students, const AcademicGroups& groups) {
    return static_cast<std::size_t>(std::count_if(students.begin(), students.end(),
        [&groups](const Student& student) { return !groups.groupFor(student); }));
}

}  // namespace AcademicProcessing
