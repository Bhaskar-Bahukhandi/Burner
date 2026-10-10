#include "RecordStorage.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr std::size_t maxRecords = 10000;

bool existsSafely(const fs::path& path, bool& found, std::string& error) {
    std::error_code ec;
    found = fs::exists(path, ec);
    if (ec) {
        error = "Cannot check " + path.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool parseCount(const std::string& line, std::size_t& count) {
    std::istringstream input(line);
    long long value;
    if (!(input >> value) || value < 0 || value > static_cast<long long>(maxRecords)) {
        return false;
    }
    input >> std::ws;
    if (!input.eof()) return false;
    count = static_cast<std::size_t>(value);
    return true;
}

bool parseStudent(const std::string& line, StudentManagement& records) {
    std::istringstream input(line);
    int roll, semester, count;
    std::string name, branch;
    if (!(input >> roll >> std::quoted(name) >> std::quoted(branch)
                >> semester >> count) || count < 1 || count > 20) {
        return false;
    }
    std::vector<int> marks;
    for (int i = 0; i < count; ++i) {
        int mark;
        if (!(input >> mark)) return false;
        marks.push_back(mark);
    }
    input >> std::ws;
    if (!input.eof()) return false;
    return records.addStudent(Student(roll, name, branch, semester, std::move(marks)));
}

bool writeRecords(const fs::path& temporary, const StudentManagement& records,
                  std::string& error) {
    std::ofstream output(temporary, std::ios::out | std::ios::trunc);
    if (!output) {
        error = "Cannot create " + temporary.string();
        return false;
    }
    output << "EDUTRACK 1\n" << records.allStudents().size() << '\n';
    for (const Student& student : records.allStudents()) {
        output << student.rollNumber() << ' ' << std::quoted(student.name())
               << ' ' << std::quoted(student.branch()) << ' ' << student.semester()
               << ' ' << student.marks().size();
        for (int mark : student.marks()) output << ' ' << mark;
        output << '\n';
    }
    output.flush();
    const bool ok = static_cast<bool>(output);
    output.close();
    if (!ok || !output) {
        error = "Could not finish writing " + temporary.string();
        return false;
    }
    return true;
}
}  // namespace

namespace RecordStorage {

bool load(const std::string& filename, StudentManagement& records, std::string& error) {
    error.clear();
    std::ifstream input(filename);
    if (!input) {
        error = "Cannot open " + filename;
        return false;
    }
    std::string line;
    if (!std::getline(input, line)) {
        error = "Missing record-file header";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "EDUTRACK 1") {
        error = "Unsupported or damaged record-file header";
        return false;
    }
    if (!std::getline(input, line)) {
        error = "Missing record count";
        return false;
    }
    std::size_t count = 0;
    if (!parseCount(line, count)) {
        error = "Invalid record count (maximum 10000)";
        return false;
    }

    StudentManagement loaded;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::getline(input, line) || !parseStudent(line, loaded)) {
            error = "Invalid or duplicate student at record " + std::to_string(i + 1);
            return false;
        }
    }
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
            error = "Unexpected data after the last student record";
            return false;
        }
    }
    if (input.bad()) {
        error = "Error while reading student records";
        return false;
    }
    records = std::move(loaded);
    return true;
}

bool save(const std::string& filename, const StudentManagement& records,
          std::string& error) {
    error.clear();
    const fs::path destination(filename);
    const fs::path temporary(filename + ".tmp");
    const fs::path backup(filename + ".bak");
    if (records.allStudents().size() > maxRecords) {
        error = "Too many records to save (maximum 10000)";
        return false;
    }

    bool hasTemporary = false, hasBackup = false, hasDestination = false;
    if (!existsSafely(temporary, hasTemporary, error) ||
        !existsSafely(backup, hasBackup, error) ||
        !existsSafely(destination, hasDestination, error)) return false;
    if (hasTemporary || hasBackup) {
        error = "An earlier temporary or backup file exists. Check students.txt.tmp "
                "and students.txt.bak before saving again.";
        return false;
    }
    if (hasDestination) {
        std::error_code typeError;
        const bool regular = fs::is_regular_file(destination, typeError);
        if (typeError) {
            error = "Cannot inspect save destination: " + typeError.message();
            return false;
        }
        if (!regular) {
            error = "Save destination is not a regular file";
            return false;
        }
    }
    if (!writeRecords(temporary, records, error)) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return false;
    }

    std::error_code ec;
    if (hasDestination) {
        fs::rename(destination, backup, ec);
        if (ec) {
            error = "Cannot back up existing records: " + ec.message();
            std::error_code ignored;
            fs::remove(temporary, ignored);
            return false;
        }
    }

    fs::rename(temporary, destination, ec);
    if (ec) {
        error = "Cannot finish saving records: " + ec.message();
        if (hasDestination) {
            std::error_code restoreError;
            fs::rename(backup, destination, restoreError);
            if (restoreError) {
                error += "; original file remains at " + backup.string() +
                         " (restore it manually before restarting)";
            }
        }
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return false;
    }
    if (hasDestination) {
        fs::remove(backup, ec);
        if (ec) {
            error = "Records saved, but could not remove " + backup.string() +
                    ": " + ec.message();
        }
    }
    return true;
}

}  // namespace RecordStorage
