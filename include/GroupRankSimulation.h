#ifndef GROUP_RANK_SIMULATION_H
#define GROUP_RANK_SIMULATION_H

#include "AcademicGroups.h"
#include "ImprovementSimulator.h"

#include <cstddef>
#include <optional>
#include <string>

namespace GroupRankSimulation {

struct Result {
    std::string groupId;
    std::size_t comparableStudents;
    std::size_t currentRank;
    std::size_t proposedRank;
    ImprovementSimulator::Result marks;
};

// Read-only and limited to students explicitly assigned to the same group.
// Returns no result when the student is unassigned or has fewer than one peer.
std::optional<Result> calculate(const StudentManagement& records,
                                const AcademicGroups& groups,
                                int rollNumber, std::size_t subjectNumber,
                                int proposedMark,
                                AcademicProcessing::GradeScale scale =
                                    AcademicProcessing::GradeScale::GeuReference);

}  // namespace GroupRankSimulation
#endif
