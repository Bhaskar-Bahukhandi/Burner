#include "GroupRankSimulation.h"
#include "RecordAlgorithms.h"

#include <set>
#include <vector>

namespace GroupRankSimulation {

std::optional<Result> calculate(const StudentManagement& records,
                                const AcademicGroups& groups,
                                int rollNumber, std::size_t subjectNumber,
                                int proposedMark,
                                AcademicProcessing::GradeScale scale) {
    const Student* target = records.findStudent(rollNumber);
    if (!target) return std::nullopt;
    const AcademicGroup* targetGroup = groups.groupFor(*target);
    if (!targetGroup) return std::nullopt;
    const auto markChange = ImprovementSimulator::simulate(*target, subjectNumber,
                                                             proposedMark, scale);
    if (!markChange) return std::nullopt;

    std::vector<Student> before;
    std::set<int> rolls;
    for (const Student& student : records.allStudents()) {
        const AcademicGroup* group = groups.groupFor(student);
        if (group && group->id == targetGroup->id) {
            if (!rolls.insert(student.rollNumber()).second) return std::nullopt;
            before.push_back(student);
        }
    }
    if (before.size() < 2) return std::nullopt;

    const auto current = RecordAlgorithms::rankOf(before, rollNumber);
    if (!current) return std::nullopt;

    std::vector<Student> after = before;
    bool replaced = false;
    for (Student& student : after) {
        if (student.rollNumber() == rollNumber) {
            std::vector<int> marks = student.marks();
            marks[subjectNumber - 1] = proposedMark;
            student = Student(student.rollNumber(), student.name(), student.branch(),
                              student.semester(), marks);
            replaced = true;
            break;
        }
    }
    if (!replaced) return std::nullopt;
    const auto projected = RecordAlgorithms::rankOf(after, rollNumber);
    if (!projected) return std::nullopt;
    return Result{targetGroup->id, before.size(), *current, *projected, *markChange};
}

}  // namespace GroupRankSimulation
