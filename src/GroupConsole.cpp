#include "GroupConsole.h"

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
