#include "SqliteStorage.h"
#include "AcademicGroups.h"
#include "StudentManagement.h"
#include "RecordStorage.h"
#include "AcademicGroupStorage.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <iterator>
#include <vector>
#include <sqlite3.h>

int main() {
    namespace fs = std::filesystem;
    const auto token = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("edutrack_sql_check_" + token);
    fs::create_directory(root);
    const std::string path = (root / "records.db").string();
    StudentManagement students;
    AcademicGroups groups;
    std::string error;
    assert(students.addStudent(Student(7, "Student One", "CSE", 3, {75, 85})));
    assert(students.addStudent(Student(8, "Student Two", "CSE", 3, {90, 65})));
    assert(SqliteStorage::save(path, students, groups, error));
    assert(!SqliteStorage::save(path, students, groups, error)); // Not an update function.

    // Save an academic group and subject order without rewriting student data.
    AcademicGroups nextGroups = groups;
    const AcademicGroup csGroup{"CS2025", "CSE", 3, "2025",
                                {{"DS", "Data Structures"}, {"MATH", "Mathematics"}}};
    assert(nextGroups.addGroup(csGroup));
    assert(SqliteStorage::commit(path, students, groups, students, nextGroups, error));
    groups = nextGroups;
    assert(groups.assign(*students.findStudent(7), "CS2025"));
    assert(SqliteStorage::commit(path, students, nextGroups, students, groups, error));
    StudentManagement loaded;
    AcademicGroups loadedGroups;
    assert(SqliteStorage::load(path, loaded, loadedGroups, error));
    assert(loaded.allStudents().size() == 2);
    assert(loaded.findStudent(7)->marks()[1] == 85);
    assert(loadedGroups.groups().size() == 1);
    assert(loadedGroups.groupFor(*loaded.findStudent(7)));
    assert(loadedGroups.groupFor(*loaded.findStudent(7))->subjects[0].code == "DS");
    assert(!loadedGroups.groupFor(*loaded.findStudent(8)));

    // A stale copy cannot overwrite a newer assignment.
    AcademicGroups stale = nextGroups;
    assert(!SqliteStorage::commit(path, students, stale, students, stale, error));
    assert(SqliteStorage::matches(path, students, groups, error));
    const AcademicGroup duplicateGroup{"CS2025", "CSE", 3, "2025", {{"X", "Other"}}};
    assert(!groups.addGroup(duplicateGroup));
    assert(groups.consistentWith(students));
    // Positional marks are named only through a matching academic group.
    std::vector<SqliteStorage::SubjectMarkRow> subjectRows;
    assert(SqliteStorage::groupSubjectMarks(path, "CS2025", subjectRows, error));
    assert(subjectRows.size() == 2 && subjectRows[0].rollNumber == 7);
    assert(subjectRows[0].subjectCode == "DS" && subjectRows[0].mark == 75);
    assert(subjectRows[1].subjectCode == "MATH" && subjectRows[1].mark == 85);
    assert(!SqliteStorage::groupSubjectMarks(path, "missing", subjectRows, error));
    assert(subjectRows.size() == 2); // Read failures leave output unchanged.
    assert(!SqliteStorage::groupSubjectMarks(path, "CS2025' OR 1=1 --", subjectRows, error));

    // An independent department's subject at position 1 must never be called DS.
    StudentManagement moreStudents = students;
    AcademicGroups moreGroups = groups;
    assert(moreStudents.addStudent(Student(9, "ECE student", "ECE", 3, {40, 50})));
    const AcademicGroup ecGroup{"EC2025", "ECE", 3, "2025",
                                {{"CIR", "Circuits"}, {"DIG", "Digital"}}};
    assert(moreGroups.addGroup(ecGroup));
    assert(moreGroups.assign(*moreStudents.findStudent(9), "EC2025"));
    assert(SqliteStorage::commit(path, students, groups, moreStudents, moreGroups, error));
    assert(SqliteStorage::groupSubjectMarks(path, "EC2025", subjectRows, error));
    assert(subjectRows.size() == 2 && subjectRows[0].subjectCode == "CIR");
    assert(subjectRows[0].mark == 40);
    assert(SqliteStorage::groupSubjectMarks(path, "CS2025", subjectRows, error));
    assert(subjectRows.size() == 2 && subjectRows[0].mark == 75);

    // Migration is staged, verified and published only to a new database.
    const std::string oldStudents = (root / "students.txt").string();
    const std::string oldGroups = (root / "academic_groups.txt").string();
    assert(RecordStorage::save(oldStudents, moreStudents, error));
    assert(AcademicGroupStorage::save(oldGroups, moreStudents, moreGroups, error));
    const auto readAll = [](const std::string& file) {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const std::string beforeStudents = readAll(oldStudents);
    const std::string beforeGroups = readAll(oldGroups);
    const std::string imported = (root / "imported.db").string();
    // A stale staging file must not be deleted by a new attempt.
    { std::ofstream stale(imported + ".importing"); stale << "do not remove"; }
    assert(!SqliteStorage::importLegacy(imported, oldStudents, oldGroups, error));
    assert(!fs::exists(imported) && readAll(imported + ".importing") == "do not remove");
    fs::remove(imported + ".importing");
    assert(SqliteStorage::importLegacy(imported, oldStudents, oldGroups, error));
    assert(fs::exists(imported) && !fs::exists(imported + ".importing"));
    assert(readAll(oldStudents) == beforeStudents && readAll(oldGroups) == beforeGroups);
    StudentManagement importedRecords;
    AcademicGroups importedGroups;
    assert(SqliteStorage::load(imported, importedRecords, importedGroups, error));
    assert(importedRecords.allStudents().size() == 3);
    assert(importedGroups.assignments().size() == 2);
    assert(!SqliteStorage::importLegacy(imported, oldStudents, oldGroups, error));
    assert(importedRecords.findStudent(9)->marks()[0] == 40);
    assert(readAll(oldStudents) == beforeStudents && readAll(oldGroups) == beforeGroups);
    // A failed parse cannot leave a half-created SQL destination.
    const std::string badSrc = (root / "bad.txt").string();
    { std::ofstream out(badSrc); out << "invalid data\n"; }
    const std::string badTarget = (root / "bad.db").string();
    assert(!SqliteStorage::importLegacy(badTarget, badSrc, oldGroups, error));
    assert(!fs::exists(badTarget) && !fs::exists(badTarget + ".importing"));

    // Bad mark ordering fails a checked load, including the group query.
    sqlite3* db = nullptr;
    assert(sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    assert(sqlite3_exec(db, "INSERT INTO marks VALUES(7,3,22)", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(db);
    assert(!SqliteStorage::groupSubjectMarks(path, "CS2025", subjectRows, error));
    assert(subjectRows.size() == 2); // last successful result retained
    fs::remove_all(root);
    std::cout << "SQLite group creation, subject order and stale-write checks passed.\n";
}
