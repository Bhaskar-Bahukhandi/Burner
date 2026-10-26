#include "AcademicGroupStorage.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace {
namespace fs = std::filesystem;

bool getLine(std::istream& input, std::string& line) {
    if (!std::getline(input, line) || line.size() > 1024) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

template <typename T>
bool exactly(std::istringstream& input, T& value) {
    if (!(input >> value)) return false;
    input >> std::ws;
    return input.eof();
}

bool countFrom(const std::string& line, std::size_t max, std::size_t& result) {
    std::istringstream input(line);
    long long value;
    if (!exactly(input, value) || value < 0 ||
        static_cast<unsigned long long>(value) > max) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

bool metadataLine(const std::string& line, AcademicGroup& group, int& count) {
    std::istringstream input(line);
    if (!(input >> std::quoted(group.id) >> std::quoted(group.branch) >>
          group.semester >> std::quoted(group.cohort) >> count)) return false;
    input >> std::ws;
    return input.eof() && count >= 1 && count <= 20;
}

bool subjectLine(const std::string& line, SubjectDefinition& subject) {
    std::istringstream input(line);
    if (!(input >> std::quoted(subject.code) >> std::quoted(subject.name))) return false;
    input >> std::ws;
    return input.eof();
}

bool assignmentLine(const std::string& line, int& roll, std::string& groupId,
                    std::string& studentName) {
    std::istringstream input(line);
    if (!(input >> roll >> std::quoted(groupId) >>
          std::quoted(studentName))) return false;
    input >> std::ws;
    return input.eof();
}

bool existsChecked(const fs::path& name, bool& present, std::string& error) {
    std::error_code ec;
    present = fs::exists(name, ec);
    if (ec) {
        error = "Cannot inspect " + name.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool writeFile(const fs::path& name, const AcademicGroups& groups,
               std::string& error) {
    std::ofstream output(name, std::ios::trunc);
    if (!output) {
        error = "Could not create " + name.string();
        return false;
    }
    output << "EDUTRACK GROUPS 1\n" << groups.groups().size() << '\n';
    for (const auto& entry : groups.groups()) {
        const AcademicGroup& group = entry.second;
        output << std::quoted(group.id) << ' ' << std::quoted(group.branch) << ' '
               << group.semester << ' ' << std::quoted(group.cohort) << ' '
               << group.subjects.size() << '\n';
        for (const SubjectDefinition& subject : group.subjects)
            output << std::quoted(subject.code) << ' '
                   << std::quoted(subject.name) << '\n';
    }
    output << groups.assignments().size() << '\n';
    for (const auto& assignment : groups.assignments())
        output << assignment.first << ' ' << std::quoted(assignment.second.groupId)
               << ' ' << std::quoted(assignment.second.studentName) << '\n';
    output.flush();
    const bool written = static_cast<bool>(output);
    output.close();
    if (!written || !output) {
        error = "Could not finish writing " + name.string();
        return false;
    }
    return true;
}
}  // namespace

namespace AcademicGroupStorage {

bool load(const std::string& filename, const StudentManagement& records,
          AcademicGroups& groups, std::string& error) {
    error.clear();
    std::ifstream input(filename);
    if (!input) {
        error = "Cannot open academic group file";
        return false;
    }
    std::string line;
    if (!getLine(input, line) || line != "EDUTRACK GROUPS 1") {
        error = "Invalid academic group file header";
        return false;
    }
    std::size_t groupCount = 0;
    if (!getLine(input, line) || !countFrom(line, 200, groupCount)) {
        error = "Invalid academic group count";
        return false;
    }
    AcademicGroups loaded;
    for (std::size_t i = 0; i < groupCount; ++i) {
        AcademicGroup group;
        int subjectCount = 0;
        if (!getLine(input, line) || !metadataLine(line, group, subjectCount)) {
            error = "Invalid group header at position " + std::to_string(i + 1);
            return false;
        }
        for (int j = 0; j < subjectCount; ++j) {
            SubjectDefinition subject;
            if (!getLine(input, line) || !subjectLine(line, subject)) {
                error = "Invalid subject definition in group " + group.id;
                return false;
            }
            group.subjects.push_back(std::move(subject));
        }
        if (!loaded.addGroup(group)) {
            error = "Duplicate or invalid academic group " + group.id;
            return false;
        }
    }
    std::size_t assignedCount = 0;
    if (!getLine(input, line) || !countFrom(line, 10000, assignedCount)) {
        error = "Invalid assignment count";
        return false;
    }
    for (std::size_t i = 0; i < assignedCount; ++i) {
        int roll = 0;
        std::string groupId, studentName;
        if (!getLine(input, line) || !assignmentLine(line, roll, groupId, studentName)) {
            error = "Invalid student assignment at position " + std::to_string(i + 1);
            return false;
        }
        const Student* student = records.findStudent(roll);
        if (!student || student->name() != studentName ||
            !loaded.findGroup(groupId) ||
            loaded.assignments().find(roll) != loaded.assignments().end() ||
            !loaded.assign(*student, groupId)) {
            error = "Unmatched, duplicate or outdated assignment for roll " +
                    std::to_string(roll);
            return false;
        }
    }
    while (getLine(input, line)) {
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
            error = "Unexpected extra data in academic group file";
            return false;
        }
    }
    if (input.bad() || !input.eof()) {
        error = "Error reading academic group file (or line too long)";
        return false;
    }
    groups = std::move(loaded);
    return true;
}

bool save(const std::string& filename, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error) {
    error.clear();
    if (!groups.consistentWith(records)) {
        error = "Some assignments no longer match student records; correct them first";
        return false;
    }
    const fs::path target(filename), temp(filename + ".tmp"), backup(filename + ".bak");
    bool targetPresent = false, tempPresent = false, backupPresent = false;
    if (!existsChecked(temp, tempPresent, error) ||
        !existsChecked(backup, backupPresent, error) ||
        !existsChecked(target, targetPresent, error)) return false;
    if (tempPresent || backupPresent) {
        error = "Previous .tmp or .bak file exists; inspect it before saving";
        return false;
    }
    if (targetPresent) {
        std::error_code ec;
        bool regular = fs::is_regular_file(target, ec);
        if (ec || !regular) {
            error = "Academic group destination is not a regular writable file";
            return false;
        }
    }
    if (!writeFile(temp, groups, error)) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        return false;
    }
    std::error_code ec;
    if (targetPresent) {
        fs::rename(target, backup, ec);
        if (ec) {
            error = "Could not back up academic group file: " + ec.message();
            std::error_code ignored;
            fs::remove(temp, ignored);
            return false;
        }
    }
    fs::rename(temp, target, ec);
    if (ec) {
        error = "Could not finish saving academic groups: " + ec.message();
        if (targetPresent) {
            std::error_code restoreError;
            fs::rename(backup, target, restoreError);
            if (restoreError) error += "; previous file remains at " + backup.string();
        }
        std::error_code ignored;
        fs::remove(temp, ignored);
        return false;
    }
    if (targetPresent) {
        fs::remove(backup, ec);
        if (ec) error = "Saved, but could not clean up backup: " + ec.message();
    }
    return true;
}

}  // namespace AcademicGroupStorage
