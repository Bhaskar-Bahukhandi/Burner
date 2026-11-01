#include "AcademicInsights.h"
#include "RecordAlgorithms.h"

#include <vector>

namespace AcademicInsights {

std::optional<Report> analyze(const StudentManagement& records,
                              const AcademicGroups& groups, int rollNumber) {
    const Student* target = records.findStudent(rollNumber);
    if (!target) return std::nullopt;
    const AcademicGroup* assigned = groups.groupFor(*target);
    if (!assigned) return std::nullopt;

    const std::size_t subjectCount = assigned->subjects.size();
    if (subjectCount == 0 || target->marks().size() != subjectCount) return std::nullopt;

    std::vector<Student> groupMembers;
    std::vector<double> peerSubjectTotals(subjectCount, 0.0);
    double peerPercentTotal = 0.0;
    std::size_t peers = 0;

    for (const Student& student : records.allStudents()) {
        const AcademicGroup* group = groups.groupFor(student);
        if (!group || group->id != assigned->id) continue;
        if (student.marks().size() != subjectCount) return std::nullopt;
        groupMembers.push_back(student);
        if (student.rollNumber() == rollNumber) continue;
        ++peers;
        peerPercentTotal += RecordAlgorithms::percentage(student);
        for (std::size_t index = 0; index < subjectCount; ++index) {
            peerSubjectTotals[index] += student.marks()[index];
        }
    }
    if (peers == 0 || groupMembers.size() != peers + 1) return std::nullopt;

    const auto rank = RecordAlgorithms::rankOf(groupMembers, rollNumber);
    if (!rank) return std::nullopt;

    Report report{rollNumber, assigned->id, groupMembers.size(), peers, *rank,
                  RecordAlgorithms::percentage(*target),
                  peerPercentTotal / static_cast<double>(peers), {},
                  std::nullopt, std::nullopt};
    report.subjects.reserve(subjectCount);
    for (std::size_t index = 0; index < subjectCount; ++index) {
        const double peerAverage = peerSubjectTotals[index] / static_cast<double>(peers);
        const int mark = target->marks()[index];
        const double difference = static_cast<double>(mark) - peerAverage;
        const SubjectDefinition& subject = assigned->subjects[index];
        report.subjects.push_back({subject.code, subject.name, mark, peerAverage,
                                   difference});

        // Ties use subject order, avoiding arbitrary or unstable output.
        if (difference < 0.0 && (!report.largestDeficit ||
            difference < report.subjects[*report.largestDeficit].difference)) {
            report.largestDeficit = index;
        }
        if (difference > 0.0 && (!report.largestLead ||
            difference > report.subjects[*report.largestLead].difference)) {
            report.largestLead = index;
        }
    }
    return report;
}

}  // namespace AcademicInsights
