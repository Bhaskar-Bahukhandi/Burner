#include "SqliteStorage.h"
#include "AcademicGroupStorage.h"
#include "RecordStorage.h"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;

// sqlite3_errmsg() is copied before closing the connection.
struct DatabaseCloser { void operator()(sqlite3* db) const { if (db) sqlite3_close(db); } };
using Database = std::unique_ptr<sqlite3, DatabaseCloser>;
struct StatementCloser { void operator()(sqlite3_stmt* stmt) const { if (stmt) sqlite3_finalize(stmt); } };
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;

[[noreturn]] void fail(sqlite3* db, const std::string& action) {
    throw std::runtime_error(action + ": " + sqlite3_errmsg(db));
}

void execute(sqlite3* db, const char* sql) {
    char* message = nullptr;
    const int code = sqlite3_exec(db, sql, nullptr, nullptr, &message);
    if (code == SQLITE_OK) return;
    std::string details = message ? message : sqlite3_errmsg(db);
    sqlite3_free(message);
    throw std::runtime_error(details);
}

Database open(const std::string& path, bool create) {
    if (path.empty()) throw std::runtime_error("Database path cannot be empty");
    sqlite3* raw = nullptr;
    const int flags = create ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                             : SQLITE_OPEN_READONLY;
    if (sqlite3_open_v2(path.c_str(), &raw, flags, nullptr) != SQLITE_OK) {
        const std::string reason = raw ? sqlite3_errmsg(raw) : "Out of memory";
        if (raw) sqlite3_close(raw);
        throw std::runtime_error("Cannot open SQLite database: " + reason);
    }
    Database db(raw);
    sqlite3_busy_timeout(db.get(), 3000);
    execute(db.get(), "PRAGMA foreign_keys = ON");
    Statement check;
    sqlite3_stmt* temp = nullptr;
    if (sqlite3_prepare_v2(db.get(), "PRAGMA foreign_keys", -1, &temp, nullptr) != SQLITE_OK)
        fail(db.get(), "Read foreign key setting");
    check.reset(temp);
    if (sqlite3_step(check.get()) != SQLITE_ROW || sqlite3_column_int(check.get(), 0) != 1)
        throw std::runtime_error("SQLite foreign key enforcement is unavailable");
    return db;
}

Statement prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK)
        fail(db, "Prepare SQL statement");
    return Statement(raw);
}

void bindInt(sqlite3* db, sqlite3_stmt* stmt, int index, int value) {
    if (sqlite3_bind_int(stmt, index, value) != SQLITE_OK) fail(db, "Bind number");
}
void bindText(sqlite3* db, sqlite3_stmt* stmt, int index, const std::string& value) {
    if (sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT)
        != SQLITE_OK) fail(db, "Bind text");
}
void writeRow(sqlite3* db, sqlite3_stmt* stmt) {
    if (sqlite3_step(stmt) != SQLITE_DONE) fail(db, "Write row");
    if (sqlite3_reset(stmt) != SQLITE_OK) fail(db, "Reset SQL statement");
    if (sqlite3_clear_bindings(stmt) != SQLITE_OK) fail(db, "Clear SQL parameters");
}
std::string columnText(sqlite3_stmt* stmt, int index) {
    const auto* bytes = sqlite3_column_text(stmt, index);
    const int size = sqlite3_column_bytes(stmt, index);
    return bytes && size >= 0 ? std::string(reinterpret_cast<const char*>(bytes),
                                            static_cast<std::size_t>(size)) : std::string();
}

int version(sqlite3* db) {
    auto stmt = prepare(db, "PRAGMA user_version");
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) fail(db, "Read schema version");
    return sqlite3_column_int(stmt.get(), 0);
}

void schema(sqlite3* db) {
    execute(db, R"SQL(
        CREATE TABLE IF NOT EXISTS students (
            roll_number INTEGER PRIMARY KEY CHECK(roll_number BETWEEN 1 AND 999999999),
            name TEXT NOT NULL CHECK(length(name) BETWEEN 1 AND 80),
            branch TEXT NOT NULL CHECK(length(branch) BETWEEN 1 AND 60),
            semester INTEGER NOT NULL CHECK(semester BETWEEN 1 AND 12)
        );
        CREATE TABLE IF NOT EXISTS marks (
            roll_number INTEGER NOT NULL REFERENCES students(roll_number) ON DELETE CASCADE,
            position INTEGER NOT NULL CHECK(position BETWEEN 1 AND 20),
            mark INTEGER NOT NULL CHECK(mark BETWEEN 0 AND 100),
            PRIMARY KEY(roll_number, position)
        );
        CREATE TABLE IF NOT EXISTS academic_groups (
            group_id TEXT PRIMARY KEY, branch TEXT NOT NULL,
            semester INTEGER NOT NULL CHECK(semester BETWEEN 1 AND 12),
            cohort TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS subjects (
            group_id TEXT NOT NULL REFERENCES academic_groups(group_id) ON DELETE CASCADE,
            position INTEGER NOT NULL CHECK(position BETWEEN 1 AND 20),
            code TEXT NOT NULL, name TEXT NOT NULL,
            PRIMARY KEY(group_id, position), UNIQUE(group_id, code)
        );
        CREATE TABLE IF NOT EXISTS group_assignments (
            roll_number INTEGER PRIMARY KEY REFERENCES students(roll_number) ON DELETE RESTRICT,
            group_id TEXT NOT NULL REFERENCES academic_groups(group_id) ON DELETE RESTRICT,
            student_name TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_assignments_group ON group_assignments(group_id);
        PRAGMA user_version = 1;
    )SQL");
}

void writeSnapshot(sqlite3* db, const StudentManagement& students, const AcademicGroups& groups) {
    if (!groups.consistentWith(students))
        throw std::runtime_error("Academic-group assignments do not match student records");
    if (students.allStudents().size() > 10000 || groups.groups().size() > 200 ||
        groups.assignments().size() > 10000)
        throw std::runtime_error("Data exceeds project limits");
    // Empty everything inside one transaction, then reinsert parent rows before
    // their children. No caller sees an intermediate state.
    execute(db, "DELETE FROM group_assignments; DELETE FROM marks; DELETE FROM subjects; "
                "DELETE FROM academic_groups; DELETE FROM students;");
    auto addStudent = prepare(db, "INSERT INTO students VALUES(?,?,?,?)");
    auto addMark = prepare(db, "INSERT INTO marks VALUES(?,?,?)");
    auto addGroup = prepare(db, "INSERT INTO academic_groups VALUES(?,?,?,?)");
    auto addSubject = prepare(db, "INSERT INTO subjects VALUES(?,?,?,?)");
    auto addAssignment = prepare(db, "INSERT INTO group_assignments VALUES(?,?,?)");
    for (const Student& student : students.allStudents()) {
        bindInt(db, addStudent.get(), 1, student.rollNumber());
        bindText(db, addStudent.get(), 2, student.name());
        bindText(db, addStudent.get(), 3, student.branch());
        bindInt(db, addStudent.get(), 4, student.semester());
        writeRow(db, addStudent.get());
        for (std::size_t i = 0; i < student.marks().size(); ++i) {
            bindInt(db, addMark.get(), 1, student.rollNumber());
            bindInt(db, addMark.get(), 2, static_cast<int>(i + 1));
            bindInt(db, addMark.get(), 3, student.marks()[i]);
            writeRow(db, addMark.get());
        }
    }
    for (const auto& pair : groups.groups()) {
        const auto& group = pair.second;
        bindText(db, addGroup.get(), 1, group.id);
        bindText(db, addGroup.get(), 2, group.branch);
        bindInt(db, addGroup.get(), 3, group.semester);
        bindText(db, addGroup.get(), 4, group.cohort);
        writeRow(db, addGroup.get());
        for (std::size_t i = 0; i < group.subjects.size(); ++i) {
            bindText(db, addSubject.get(), 1, group.id);
            bindInt(db, addSubject.get(), 2, static_cast<int>(i + 1));
            bindText(db, addSubject.get(), 3, group.subjects[i].code);
            bindText(db, addSubject.get(), 4, group.subjects[i].name);
            writeRow(db, addSubject.get());
        }
    }
    for (const auto& pair : groups.assignments()) {
        bindInt(db, addAssignment.get(), 1, pair.first);
        bindText(db, addAssignment.get(), 2, pair.second.groupId);
        bindText(db, addAssignment.get(), 3, pair.second.studentName);
        writeRow(db, addAssignment.get());
    }
}

void ensureProjectDatabase(sqlite3* db, bool allowNew) {
    const int current = version(db);
    if (current == 1) {
        // A version number alone does not make an arbitrary DB an EduTrack DB.
        for (const char* table : {"students", "marks", "academic_groups",
                                  "subjects", "group_assignments"}) {
            auto found = prepare(db,
                "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
            bindText(db, found.get(), 1, table);
            if (sqlite3_step(found.get()) != SQLITE_ROW)
                throw std::runtime_error(std::string("Expected SQL table is missing: ") + table);
        }
        return;
    }
    if (current != 0 || !allowNew)
        throw std::runtime_error("Unsupported or uninitialized EduTrack SQL schema version");
    // Do not silently take over a different SQLite database whose version is 0.
    auto stmt = prepare(db, "SELECT COUNT(*) FROM sqlite_master "
                            "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%'");
    if (sqlite3_step(stmt.get()) != SQLITE_ROW || sqlite3_column_int(stmt.get(), 0) != 0)
        throw std::runtime_error("The SQL file already contains another schema");
}

void ensureNoRecoveryFiles(const std::string& file) {
    for (const char* suffix : {".tmp", ".bak"}) {
        std::error_code ec;
        const bool present = fs::exists(file + suffix, ec);
        if (ec) throw std::runtime_error("Cannot inspect recovery file: " + ec.message());
        if (present) throw std::runtime_error("Recovery file exists: " + file + suffix);
    }
}

} // namespace

namespace SqliteStorage {

bool save(const std::string& path, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error) {
    error.clear();
    try {
        auto db = open(path, true);
        ensureProjectDatabase(db.get(), true);
        execute(db.get(), "PRAGMA synchronous = FULL");
        execute(db.get(), "BEGIN IMMEDIATE");
        try {
            schema(db.get());
            writeSnapshot(db.get(), records, groups);
            execute(db.get(), "COMMIT");
        } catch (...) {
            // A failed transaction preserves the last committed snapshot.
            try { execute(db.get(), "ROLLBACK"); } catch (...) { }
            throw;
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

bool load(const std::string& path, StudentManagement& records,
          AcademicGroups& groups, std::string& error) {
    error.clear();
    try {
        auto db = open(path, false);
        ensureProjectDatabase(db.get(), false);
        StudentManagement stagedRecords;
        AcademicGroups stagedGroups;
        execute(db.get(), "BEGIN");
        try {
            // One consistent SQL read snapshot, even if another process writes.
            auto readStudents = prepare(db.get(),
                "SELECT roll_number,name,branch,semester FROM students ORDER BY roll_number");
            std::size_t studentCount = 0;
            while (true) {
                int rc = sqlite3_step(readStudents.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db.get(), "Read students");
                const int roll = sqlite3_column_int(readStudents.get(), 0);
                auto readMarks = prepare(db.get(),
                    "SELECT position,mark FROM marks WHERE roll_number=? ORDER BY position");
                bindInt(db.get(), readMarks.get(), 1, roll);
                std::vector<int> marks;
                while (true) {
                    rc = sqlite3_step(readMarks.get());
                    if (rc == SQLITE_DONE) break;
                    if (rc != SQLITE_ROW) fail(db.get(), "Read marks");
                    if (sqlite3_column_int(readMarks.get(), 0) != static_cast<int>(marks.size()+1))
                        throw std::runtime_error("Non-contiguous subject marks for roll " + std::to_string(roll));
                    marks.push_back(sqlite3_column_int(readMarks.get(), 1));
                }
                if (!stagedRecords.addStudent(Student(roll, columnText(readStudents.get(), 1),
                      columnText(readStudents.get(), 2), sqlite3_column_int(readStudents.get(), 3),
                      std::move(marks))))
                    throw std::runtime_error("Invalid student record in SQL database");
                if (++studentCount > 10000) throw std::runtime_error("Too many students");
            }
            auto readGroups = prepare(db.get(),
                "SELECT group_id, branch, semester, cohort FROM academic_groups ORDER BY group_id");
            std::size_t groupCount = 0;
            while (true) {
                int rc = sqlite3_step(readGroups.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db.get(), "Read academic groups");
                AcademicGroup group{columnText(readGroups.get(), 0),columnText(readGroups.get(), 1),
                                    sqlite3_column_int(readGroups.get(),2),columnText(readGroups.get(),3),{}};
                auto readSubjects = prepare(db.get(),
                    "SELECT position,code,name FROM subjects WHERE group_id=? ORDER BY position");
                bindText(db.get(), readSubjects.get(), 1, group.id);
                while (true) {
                    rc = sqlite3_step(readSubjects.get());
                    if (rc == SQLITE_DONE) break;
                    if (rc != SQLITE_ROW) fail(db.get(), "Read subjects");
                    if (sqlite3_column_int(readSubjects.get(), 0) !=
                        static_cast<int>(group.subjects.size()+1))
                        throw std::runtime_error("Non-contiguous subject codes in group " + group.id);
                    group.subjects.push_back({columnText(readSubjects.get(), 1),
                                              columnText(readSubjects.get(), 2)});
                }
                if (!stagedGroups.addGroup(group))
                    throw std::runtime_error("Invalid group definition in SQL database");
                if (++groupCount > 200) throw std::runtime_error("Too many academic groups");
            }
            auto readAssignments = prepare(db.get(),
                "SELECT roll_number,group_id,student_name FROM group_assignments ORDER BY roll_number");
            while (true) {
                int rc = sqlite3_step(readAssignments.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db.get(), "Read group assignments");
                const int roll = sqlite3_column_int(readAssignments.get(), 0);
                const std::string groupId = columnText(readAssignments.get(), 1);
                const std::string assignedName = columnText(readAssignments.get(), 2);
                const Student* student = stagedRecords.findStudent(roll);
                if (!student || student->name() != assignedName ||
                    stagedGroups.assignments().find(roll) != stagedGroups.assignments().end() ||
                    !stagedGroups.assign(*student, groupId))
                    throw std::runtime_error("Outdated group assignment in SQL database");
            }
            if (!stagedGroups.consistentWith(stagedRecords))
                throw std::runtime_error("SQL records have inconsistent academic groups");
            execute(db.get(), "COMMIT");
        } catch (...) {
            try { execute(db.get(), "ROLLBACK"); } catch (...) { }
            throw;
        }
        records = std::move(stagedRecords);
        groups = std::move(stagedGroups);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

bool importLegacy(const std::string& databaseFile, const std::string& studentsFile,
                  const std::string& groupsFile, std::string& error) {
    error.clear();
    try {
        std::error_code ec;
        const bool databaseExists = fs::exists(databaseFile, ec);
        if (ec) throw std::runtime_error("Cannot inspect SQL target: " + ec.message());
        if (databaseExists) throw std::runtime_error("SQL target exists; refusing to overwrite it");
        ensureNoRecoveryFiles(studentsFile);
        ensureNoRecoveryFiles(groupsFile);
        const bool haveStudents = fs::exists(studentsFile, ec);
        if (ec) throw std::runtime_error("Cannot inspect student source: " + ec.message());
        if (!haveStudents) throw std::runtime_error("Legacy students file is missing");
        StudentManagement records;
        if (!RecordStorage::load(studentsFile, records, error)) return false;
        AcademicGroups groups;
        const bool haveGroups = fs::exists(groupsFile, ec);
        if (ec) throw std::runtime_error("Cannot inspect group source: " + ec.message());
        if (haveGroups && !AcademicGroupStorage::load(groupsFile, records, groups, error))
            return false;
        if (!groups.consistentWith(records))
            throw std::runtime_error("Existing group assignments do not match students");
        if (!save(databaseFile, records, groups, error)) return false;
        StudentManagement checkRecords;
        AcademicGroups checkGroups;
        if (!load(databaseFile, checkRecords, checkGroups, error)) return false;
        if (checkRecords.allStudents().size() != records.allStudents().size() ||
            checkGroups.groups().size() != groups.groups().size() ||
            checkGroups.assignments().size() != groups.assignments().size())
            throw std::runtime_error("Imported SQL record counts differ from legacy data");
        // Compare every value, not only totals, before declaring migration ready.
        for (const Student& original : records.allStudents()) {
            const Student* loaded = checkRecords.findStudent(original.rollNumber());
            if (!loaded || loaded->name() != original.name() ||
                loaded->branch() != original.branch() ||
                loaded->semester() != original.semester() ||
                loaded->marks() != original.marks())
                throw std::runtime_error("Student data changed during SQL migration");
        }
        const auto& originalGroups = groups.groups();
        const auto& sqlGroups = checkGroups.groups();
        for (const auto& entry : originalGroups) {
            const auto found = sqlGroups.find(entry.first);
            if (found == sqlGroups.end())
                throw std::runtime_error("Academic group missing after import");
            const AcademicGroup& old = entry.second;
            const AcademicGroup& now = found->second;
            if (old.id != now.id || old.branch != now.branch ||
                old.semester != now.semester || old.cohort != now.cohort ||
                old.subjects.size() != now.subjects.size())
                throw std::runtime_error("Academic group metadata changed during import");
            for (std::size_t i = 0; i < old.subjects.size(); ++i) {
                if (old.subjects[i].code != now.subjects[i].code ||
                    old.subjects[i].name != now.subjects[i].name)
                    throw std::runtime_error("Subject metadata changed during import");
            }
        }
        for (const auto& entry : groups.assignments()) {
            const auto found = checkGroups.assignments().find(entry.first);
            if (found == checkGroups.assignments().end() ||
                found->second.groupId != entry.second.groupId ||
                found->second.studentName != entry.second.studentName)
                throw std::runtime_error("Group assignments changed during SQL migration");
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

} // namespace SqliteStorage
