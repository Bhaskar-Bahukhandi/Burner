#ifndef ACADEMIC_INSIGHTS_H
#define ACADEMIC_INSIGHTS_H

#include "AcademicGroups.h"
#include "StudentManagement.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace AcademicInsights {

struct SubjectInsight {
    std::string code;
    std::string name;
    int mark;
    double peerAverage;
    double difference; // Student mark minus peer average, in marks out of 100.
};

struct Report {
    int rollNumber;
    std::string groupId;
    std::size_t groupSize;
    std::size_t peerCount;
    std::size_t groupRank;
    double studentPercentage;
    double peerPercentageAverage;
    std::vector<SubjectInsight> subjects;
    std::optional<std::size_t> largestDeficit; // Subject index; only if below peers.
    std::optional<std::size_t> largestLead;    // Subject index; only if above peers.
};

// Compares the selected student only with other valid members of the same
// explicitly assigned group. At least one peer is required. No data is changed.
// 'Peer average' deliberately excludes the selected student's own marks.
std::optional<Report> analyze(const StudentManagement& records,
                              const AcademicGroups& groups, int rollNumber);

}  // namespace AcademicInsights

#endif
