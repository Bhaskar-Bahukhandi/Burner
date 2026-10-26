#include "GroupConsole.h"
#include "AcademicInsights.h"
#include "ImprovementGoalPlanner.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace {
bool line(std::istream& input, std::ostream& output, const std::string& prompt,
          std::string& result) {
    output << prompt;
    return static_cast<bool>(std::getline(input, result));
}
bool number(std::istream& input, std::ostream& output, const std::string& prompt,
            int min, int max, int& result) {
    std::string value;
    while (line(input, output, prompt, value)) {
        std::istringstream parsed(value);
        char extra = 0;
        if ((parsed >> result) && !(parsed >> extra) && result >= min && result <= max)
            return true;
        output << "Enter a whole number from " << min << " to " << max << ".\n";
    }
    return false;
}
bool percentageHundredths(std::istream& input, std::ostream& output, int& value) {
    std::string raw;
    while (line(input, output, "Target percentage (0 to 100; up to 2 decimals): ", raw)) {
        // Parse decimal text exactly. A double followed by rounding would
        // sometimes accept a result slightly below the requested goal.
        if (!raw.empty() && raw.size() <= 6) {
            std::size_t dot = raw.find('.');
            if (dot == std::string::npos) dot = raw.size();
            if (dot > 0 && dot <= 3 && raw.size() - dot <= 3 &&
                (dot == raw.size() || raw.size() > dot + 1)) {
                bool valid = true;
                for (std::size_t i = 0; i < raw.size(); ++i) {
                    if (i == dot) continue;
                    if (raw[i] < '0' || raw[i] > '9') valid = false;
                }
                if (valid) {
                    int whole = 0, fraction = 0;
                    for (std::size_t i = 0; i < dot; ++i) whole = whole * 10 + (raw[i] - '0');
                    if (dot + 1 < raw.size()) {
                        fraction = (raw[dot + 1] - '0') * 10;
                        if (dot + 2 < raw.size()) fraction += raw[dot + 2] - '0';
                    }
                    value = whole * 100 + fraction;
                    if (value <= 10000) return true;
                }
            }
        }
        output << "Enter a percentage between 0 and 100, with up to two decimals.\n";
    }
    return false;
}

bool printGoal(std::istream& input, std::ostream& output,
               const StudentManagement& records, const AcademicGroups& groups,
               AcademicProcessing::GradeScale scale) {
    int roll = 0;
    if (!number(input, output, "Student roll number: ", 1, 999999999, roll)) return false;
    const Student* student = records.findStudent(roll);
    if (!student) { output << "Student not found.\n"; return true; }
    int kind = 0;
    if (!number(input, output, "Goal: 1. Target percentage  2. Target grade: ", 1, 2, kind))
        return false;
    int hundredths = 0;
    std::string grade;
    if (kind == 1) {
        if (!percentageHundredths(input, output, hundredths)) return false;
    } else {
        if (scale == AcademicProcessing::GradeScale::GeuReference)
            output << "Illustrative reference grades: O, A+, A, B+, B, C, P.\n";
        else output << "Original project grades: A, B, C, D.\n";
        if (!line(input, output, "Desired grade (exact uppercase code): ", grade)) return false;
    }
    int limitChoice = 0;
    if (!number(input, output, "Ceilings: 1. Up to 100 each  2. Set subject ceilings: ",
                1, 2, limitChoice)) return false;
    std::vector<int> ceilings;
    const AcademicGroup* group = groups.groupFor(*student);
    if (limitChoice == 2) {
        for (std::size_t i = 0; i < student->marks().size(); ++i) {
            const int current = student->marks()[i];
            const std::string label = group ? group->subjects[i].name :
                                      "Subject " + std::to_string(i + 1);
            int ceiling = 100;
            if (!number(input, output, label + " ceiling (" +
                    std::to_string(current) + "-100): ", current, 100, ceiling)) return false;
            ceilings.push_back(ceiling);
        }
    }
    const auto result = kind == 1
        ? ImprovementGoalPlanner::forPercentage(*student, hundredths, scale, ceilings)
        : ImprovementGoalPlanner::forGrade(*student, grade, scale, ceilings);
    if (!result) {
        output << "Invalid goal or subject limits. No records changed.\n";
        return true;
    }
    output << "\nHypothetical improvement plan — saved marks are unchanged.\n"
           << "Grade scheme: " << AcademicProcessing::scaleName(scale) << "\n"
           << std::fixed << std::setprecision(2)
           << "Current: " << result->currentTotal << " total, "
           << result->currentPercentage << "%, " << result->currentGrade.letter << "\n"
           << "Minimum total for requested goal: " << result->requiredTotal << "\n"
           << "Maximum with chosen ceilings: " << result->maximumTotal << "\n";
    if (result->status == ImprovementGoalPlanner::Status::AlreadyMet)
        output << "Status: target already met; no increase needed.\n";
    else if (result->status == ImprovementGoalPlanner::Status::Unreachable)
        output << "Status: UNREACHABLE under these ceilings. Maximum possible "
               << result->maximumTotal << ", short by "
               << result->requiredTotal - result->maximumTotal << " marks.\n";
    else output << "Status: target mathematically achievable under these ceilings.\n";
    output << "Proposed total: " << result->proposedTotal << " ("
           << result->proposedPercentage << "%), illustrative grade "
           << result->proposedGrade.letter << "\n";
    for (const auto& subject : result->subjects) {
        const std::string label = group ? group->subjects[subject.number - 1].name :
                                  "Subject " + std::to_string(subject.number);
        output << "  " << label << ": " << subject.currentMark << " -> "
               << subject.proposedMark << " (ceiling " << subject.maximumMark
               << ", +" << subject.proposedMark - subject.currentMark << ")\n";
    }
    output << "The algorithm balances the lowest hypothetical marks first. "
              "This is not a prediction, a study-effort estimate, an SGPA or "
              "an official university grade.\n";
    return true;
}

bool saveChange(const std::string& filename, const StudentManagement& records,
                AcademicGroups& groups, AcademicGroups& proposed,
                std::ostream& output) {
    std::string error;
    if (!AcademicGroupStorage::save(filename, records, proposed, error)) {
        output << "Group change was NOT applied: " << error << "\n";
        return true;
    }
    groups = std::move(proposed);
    output << "Academic group information saved.\n";
    if (!error.empty()) output << "Note: " << error << "\n";
    return true;
}
}

namespace GroupConsole {
bool handle(int choice, const StudentManagement& records, AcademicGroups& groups,
            const std::string& filename, AcademicProcessing::GradeScale scale,
            std::istream& input, std::ostream& output) {
    if (choice == 27) return printGoal(input, output, records, groups, scale);
    if (choice == 26) {
        int roll = 0;
        if (!number(input, output, "Student roll number: ", 1, 999999999, roll))
            return false;
        const Student* student = records.findStudent(roll);
        if (!student) {
            output << "Student not found.\n";
            return true;
        }
        if (!groups.groupFor(*student)) {
            output << "Student has no valid academic group assignment.\n";
            return true;
        }
        const auto report = AcademicInsights::analyze(records, groups, roll);
        if (!report) {
            output << "At least one other student in the same valid academic group "
                      "is needed for a comparison.\n";
            return true;
        }
        output << "\nAcademic insights (current marks only; not a prediction)\n"
               << "Group: " << report->groupId << " | Group members: "
               << report->groupSize << " | Other students: " << report->peerCount
               << " | Group rank: " << report->groupRank << "\n"
               << std::fixed << std::setprecision(2)
               << "Student average: " << report->studentPercentage
               << "% | Other students' average: " << report->peerPercentageAverage
               << "%\n";
        for (const auto& subject : report->subjects) {
            output << subject.code << " (" << subject.name << "): "
                   << subject.mark << "/100, other students' average "
                   << subject.peerAverage << "/100, gap "
                   << std::showpos << subject.difference << std::noshowpos
                   << " marks\n";
        }
        if (report->largestDeficit) {
            const auto& subject = report->subjects[*report->largestDeficit];
            output << "Largest gap below peers: " << subject.name << " ("
                   << -subject.difference << " marks below).\n";
        } else {
            output << "No subject is below the other students' average.\n";
        }
        if (report->largestLead) {
            const auto& subject = report->subjects[*report->largestLead];
            output << "Largest gap above peers: " << subject.name << " ("
                   << subject.difference << " marks above).\n";
        } else {
            output << "No subject is above the other students' average.\n";
        }
        if (report->peerCount < 3) {
            output << "Note: This comparison group is small; the averages "
                      "may not represent a wider class.\n";
        }
        output << "These are numerical comparisons, not causes, forecasts or "
                  "study recommendations.\n";
        return true;
    }
    if (choice == 24) {
        if (groups.groups().empty()) output << "No academic groups yet.\n";
        for (const auto& item : groups.groups()) {
            const AcademicGroup& group = item.second;
            output << "Group " << group.id << ": " << group.branch << ", semester "
                   << group.semester << ", cohort " << group.cohort << "\n";
            for (std::size_t i = 0; i < group.subjects.size(); ++i) {
                output << "  Subject " << (i + 1) << ": " << group.subjects[i].code
                       << " - " << group.subjects[i].name << "\n";
            }
            std::size_t assigned = 0;
            for (const auto& entry : groups.assignments()) {
                const Student* student = records.findStudent(entry.first);
                const AcademicGroup* active = student ? groups.groupFor(*student) : nullptr;
                if (active && active->id == group.id) ++assigned;
            }
            output << "  Assigned students: " << assigned << "\n";
        }
        return true;
    }
    if (choice == 21) {
        AcademicGroup group;
        if (!line(input, output, "Group ID (uppercase letters, digits, _ or -): ", group.id) ||
            !line(input, output, "Branch (exactly as on student records): ", group.branch))
            return false;
        if (!number(input, output, "Semester (1-12): ", 1, 12, group.semester) ||
            !line(input, output, "Cohort year (e.g. 2025): ", group.cohort)) return false;
        int count = 0;
        if (!number(input, output, "Number of subjects (1-20): ", 1, 20, count)) return false;
        for (int i = 0; i < count; ++i) {
            SubjectDefinition subject;
            if (!line(input, output, "Subject " + std::to_string(i + 1) + " code: ",
                      subject.code) ||
                !line(input, output, "Subject " + std::to_string(i + 1) + " name: ",
                      subject.name)) return false;
            group.subjects.push_back(std::move(subject));
        }
        AcademicGroups proposed = groups;
        if (!proposed.addGroup(group)) {
            output << "Invalid or duplicate group. Check ID, subjects and semester.\n";
            return true;
        }
        return saveChange(filename, records, groups, proposed, output);
    }
    if (choice == 22 || choice == 23 || choice == 25) {
        int roll = 0;
        if (!number(input, output, "Student roll number: ", 1, 999999999, roll)) return false;
        const Student* student = records.findStudent(roll);
        if (!student) { output << "Student not found.\n"; return true; }
        if (choice == 23) {
            AcademicGroups proposed = groups;
            if (!proposed.unassign(roll)) {
                output << "This student has no group assignment.\n";
                return true;
            }
            return saveChange(filename, records, groups, proposed, output);
        }
        if (choice == 22) {
            std::string id;
            if (!line(input, output, "Group ID: ", id)) return false;
            AcademicGroups proposed = groups;
            if (!proposed.assign(*student, id)) {
                output << "Assignment failed: verify branch, semester and subject count.\n";
                return true;
            }
            return saveChange(filename, records, groups, proposed, output);
        }
        int subject = 0, mark = 0;
        if (!number(input, output, "Subject position (1-20): ", 1, 20, subject) ||
            !number(input, output, "Hypothetical mark (0-100): ", 0, 100, mark))
            return false;
        const auto result = GroupRankSimulation::calculate(records, groups, roll,
                                static_cast<std::size_t>(subject), mark, scale);
        if (!result) {
            output << "Cannot compare: check group assignments, subject number, "
                      "or whether another comparable student exists.\n";
            return true;
        }
        output << "Hypothetical result only. Real marks are unchanged.\n"
               << "Group: " << result->groupId << " | Comparable students: "
               << result->comparableStudents << "\n"
               << "Subject " << result->marks.subjectNumber << ": "
               << result->marks.originalMark << " -> " << result->marks.proposedMark << "\n"
               << "Percentage: " << std::fixed << std::setprecision(2)
               << result->marks.originalPercentage << "% -> "
               << result->marks.projectedPercentage << "%\n"
               << "Grade: " << result->marks.originalGrade.letter << " -> "
               << result->marks.projectedGrade.letter << "\n"
               << "Group rank: " << result->currentRank << " -> "
               << result->proposedRank << "\n";
        return true;
    }
    return true;
}
}
