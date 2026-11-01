#include "SqliteStorage.h"
#include "AcademicGroupStorage.h"
#include "RecordStorage.h"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <unordered_map>
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

Database open(const std::string& path, bool create, bool write = false) {
    if (path.empty()) throw std::runtime_error("Database path cannot be empty");
    sqlite3* raw = nullptr;
    const int flags = create ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                             : write ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
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

void verifyIntegrity(sqlite3* db) {
    // Foreign-key checks catch orphan rows introduced by an external program
    // with PRAGMA foreign_keys=OFF. Reject them before loading or editing.
    auto check = prepare(db, "PRAGMA quick_check");
    if (sqlite3_step(check.get()) != SQLITE_ROW ||
        columnText(check.get(), 0) != "ok" ||
        sqlite3_step(check.get()) != SQLITE_DONE)
        throw std::runtime_error("SQLite quick_check failed; inspect the database backup");
    auto foreign = prepare(db, "PRAGMA foreign_key_check");
    const int result = sqlite3_step(foreign.get());
    if (result != SQLITE_DONE) {
        if (result != SQLITE_ROW) fail(db, "Check SQL foreign keys");
        throw std::runtime_error("SQLite foreign key violations detected");
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

// A first-run initializer must never interpret an old database (or one of
// its WAL/journal files) as permission to create/rewrite data. Use
// symlink_status rather than exists() so a dangling symlink is also rejected.
void requireNewDatabaseTarget(const std::string& path) {
    if (path.empty()) throw std::runtime_error("Database path cannot be empty");
    for (const std::string& name : {path, path + "-wal", path + "-shm", path + "-journal"}) {
        std::error_code ec;
        const auto status = fs::symlink_status(name, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
            throw std::runtime_error("Cannot inspect SQL target: " + ec.message());
        if (status.type() != fs::file_type::not_found)
            throw std::runtime_error("SQL target or recovery sidecar exists; refusing to overwrite: " + name);
    }
}

void ensureNoRecoveryFiles(const std::string& file) {
    for (const char* suffix : {".tmp", ".bak"}) {
        std::error_code ec;
        const bool present = fs::exists(file + suffix, ec);
        if (ec) throw std::runtime_error("Cannot inspect recovery file: " + ec.message());
        if (present) throw std::runtime_error("Recovery file exists: " + file + suffix);
    }
}

// Reconstruct a consistent checked snapshot using this connection. Callers
// wrap this reader in a read or write transaction.
void readSnapshot(sqlite3* db, StudentManagement& stagedRecords,
                  AcademicGroups& stagedGroups) {
            // One consistent SQL read snapshot, even if another process writes.
            auto readStudents = prepare(db,
                "SELECT roll_number,name,branch,semester FROM students ORDER BY roll_number");
            std::size_t studentCount = 0;
            while (true) {
                int rc = sqlite3_step(readStudents.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db, "Read students");
                const int roll = sqlite3_column_int(readStudents.get(), 0);
                auto readMarks = prepare(db,
                    "SELECT position,mark FROM marks WHERE roll_number=? ORDER BY position");
                bindInt(db, readMarks.get(), 1, roll);
                std::vector<int> marks;
                while (true) {
                    rc = sqlite3_step(readMarks.get());
                    if (rc == SQLITE_DONE) break;
                    if (rc != SQLITE_ROW) fail(db, "Read marks");
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
            auto readGroups = prepare(db,
                "SELECT group_id, branch, semester, cohort FROM academic_groups ORDER BY group_id");
            std::size_t groupCount = 0;
            while (true) {
                int rc = sqlite3_step(readGroups.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db, "Read academic groups");
                AcademicGroup group{columnText(readGroups.get(), 0),columnText(readGroups.get(), 1),
                                    sqlite3_column_int(readGroups.get(),2),columnText(readGroups.get(),3),{}};
                auto readSubjects = prepare(db,
                    "SELECT position,code,name FROM subjects WHERE group_id=? ORDER BY position");
                bindText(db, readSubjects.get(), 1, group.id);
                while (true) {
                    rc = sqlite3_step(readSubjects.get());
                    if (rc == SQLITE_DONE) break;
                    if (rc != SQLITE_ROW) fail(db, "Read subjects");
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
            auto readAssignments = prepare(db,
                "SELECT roll_number,group_id,student_name FROM group_assignments ORDER BY roll_number");
            while (true) {
                int rc = sqlite3_step(readAssignments.get());
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) fail(db, "Read group assignments");
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
 }

bool equalRecords(const StudentManagement& a, const StudentManagement& b) {
    if (a.allStudents().size() != b.allStudents().size()) return false;
    std::unordered_map<int, const Student*> lookup;
    lookup.reserve(b.allStudents().size());
    for (const Student& value : b.allStudents()) lookup.emplace(value.rollNumber(), &value);
    for (const Student& x : a.allStudents()) {
        const auto found = lookup.find(x.rollNumber());
        if (found == lookup.end()) return false;
        const Student* y = found->second;
        if (x.name() != y->name() || x.branch() != y->branch() ||
            x.semester() != y->semester() || x.marks() != y->marks()) return false;
    }
    return true;
}

bool equalGroups(const AcademicGroups& a, const AcademicGroups& b) {
    if (a.assignments().size() != b.assignments().size() ||
        a.groups().size() != b.groups().size()) return false;
    for (const auto& pair : a.assignments()) {
        auto found = b.assignments().find(pair.first);
        if (found == b.assignments().end() ||
            pair.second.groupId != found->second.groupId ||
            pair.second.studentName != found->second.studentName) return false;
    }
    for (const auto& pair : a.groups()) {
        auto found = b.groups().find(pair.first);
        if (found == b.groups().end()) return false;
        const auto& x = pair.second;
        const auto& y = found->second;
        if (x.id != y.id || x.branch != y.branch || x.semester != y.semester ||
            x.cohort != y.cohort || x.subjects.size() != y.subjects.size()) return false;
        for (std::size_t i = 0; i < x.subjects.size(); ++i)
            if (x.subjects[i].code != y.subjects[i].code ||
                x.subjects[i].name != y.subjects[i].name) return false;
    }
    return true;
}

void writeDelta(sqlite3* db, const StudentManagement& previous,
                const AcademicGroups& previousGroups,
                const StudentManagement& current,
                const AcademicGroups& currentGroups) {
    if (!currentGroups.consistentWith(current) ||
        current.allStudents().size() > 10000 || currentGroups.groups().size() > 200 ||
        currentGroups.assignments().size() > 10000)
        throw std::runtime_error("Invalid project state; no SQL changes applied");
    std::unordered_map<int, const Student*> previousByRoll, currentByRoll;
    previousByRoll.reserve(previous.allStudents().size());
    currentByRoll.reserve(current.allStudents().size());
    for (const Student& student : previous.allStudents())
        previousByRoll.emplace(student.rollNumber(), &student);
    for (const Student& student : current.allStudents())
        currentByRoll.emplace(student.rollNumber(), &student);
    // Remove only assignments whose association changed or whose definition
    // is being replaced. This preserves all unrelated rows and their keys.
    auto delAssignment = prepare(db, "DELETE FROM group_assignments WHERE roll_number=?");
    auto delStudent = prepare(db, "DELETE FROM students WHERE roll_number=?");
    auto delMarks = prepare(db, "DELETE FROM marks WHERE roll_number=?");
    auto delGroup = prepare(db, "DELETE FROM academic_groups WHERE group_id=?");
    auto addStudent = prepare(db, "INSERT INTO students VALUES(?,?,?,?)");
    auto updateStudent = prepare(db, "UPDATE students SET name=?,branch=?,semester=? WHERE roll_number=?");
    auto addMark = prepare(db, "INSERT INTO marks VALUES(?,?,?)");
    auto addGroup = prepare(db, "INSERT INTO academic_groups VALUES(?,?,?,?)");
    auto addSubject = prepare(db, "INSERT INTO subjects VALUES(?,?,?,?)");
    auto addAssignment = prepare(db, "INSERT INTO group_assignments VALUES(?,?,?)");
    const auto groupChanged = [&](const std::string& id) {
        const auto* before = previousGroups.findGroup(id);
        const auto* after = currentGroups.findGroup(id);
        if (!before || !after) return before != after;
        if (before->branch != after->branch || before->semester != after->semester ||
            before->cohort != after->cohort || before->subjects.size() != after->subjects.size())
            return true;
        for (std::size_t i = 0; i < before->subjects.size(); ++i)
            if (before->subjects[i].code != after->subjects[i].code ||
                before->subjects[i].name != after->subjects[i].name) return true;
        return false;
    };
    const auto assignmentChanged = [&](int roll, const GroupAssignment& before) {
        const auto found = currentGroups.assignments().find(roll);
        return found == currentGroups.assignments().end() ||
            before.groupId != found->second.groupId ||
            before.studentName != found->second.studentName || groupChanged(before.groupId);
    };
    for (const auto& item : previousGroups.assignments()) {
        if (!assignmentChanged(item.first, item.second)) continue;
        bindInt(db, delAssignment.get(), 1, item.first);
        writeRow(db, delAssignment.get());
    }
    for (const Student& student : previous.allStudents()) {
        if (currentByRoll.find(student.rollNumber()) != currentByRoll.end()) continue;
        bindInt(db, delStudent.get(), 1, student.rollNumber());
        writeRow(db, delStudent.get());
    }
    for (const auto& item : previousGroups.groups()) {
        if (!groupChanged(item.first)) continue;
        bindText(db, delGroup.get(), 1, item.first);
        writeRow(db, delGroup.get());
    }
    for (const Student& student : current.allStudents()) {
        const auto oldIt = previousByRoll.find(student.rollNumber());
        const Student* earlier = oldIt == previousByRoll.end() ? nullptr : oldIt->second;
        if (earlier && earlier->name() == student.name() &&
            earlier->branch() == student.branch() &&
            earlier->semester() == student.semester() &&
            earlier->marks() == student.marks()) continue;
        const bool marksChanged = !earlier || earlier->marks() != student.marks();
        const bool detailsChanged = !earlier || earlier->name() != student.name() ||
                                    earlier->branch() != student.branch() ||
                                    earlier->semester() != student.semester();
        if (!earlier) {
            bindInt(db, addStudent.get(), 1, student.rollNumber());
            bindText(db, addStudent.get(), 2, student.name());
            bindText(db, addStudent.get(), 3, student.branch());
            bindInt(db, addStudent.get(), 4, student.semester());
            writeRow(db, addStudent.get());
        } else {
            if (detailsChanged) {
                bindText(db, updateStudent.get(), 1, student.name());
                bindText(db, updateStudent.get(), 2, student.branch());
                bindInt(db, updateStudent.get(), 3, student.semester());
                bindInt(db, updateStudent.get(), 4, student.rollNumber());
                writeRow(db, updateStudent.get());
            }
            if (marksChanged) {
                bindInt(db, delMarks.get(), 1, student.rollNumber());
                writeRow(db, delMarks.get());
            }
        }
        if (marksChanged) for (std::size_t i = 0; i < student.marks().size(); ++i) {
            bindInt(db, addMark.get(), 1, student.rollNumber());
            bindInt(db, addMark.get(), 2, static_cast<int>(i + 1));
            bindInt(db, addMark.get(), 3, student.marks()[i]);
            writeRow(db, addMark.get());
        }
    }
    for (const auto& item : currentGroups.groups()) {
        if (!groupChanged(item.first)) continue;
        const auto& g = item.second;
        bindText(db, addGroup.get(), 1, g.id);
        bindText(db, addGroup.get(), 2, g.branch);
        bindInt(db, addGroup.get(), 3, g.semester);
        bindText(db, addGroup.get(), 4, g.cohort);
        writeRow(db, addGroup.get());
        for (std::size_t i = 0; i < g.subjects.size(); ++i) {
            bindText(db, addSubject.get(), 1, g.id);
            bindInt(db, addSubject.get(), 2, static_cast<int>(i + 1));
            bindText(db, addSubject.get(), 3, g.subjects[i].code);
            bindText(db, addSubject.get(), 4, g.subjects[i].name);
            writeRow(db, addSubject.get());
        }
    }
    for (const auto& item : currentGroups.assignments()) {
        auto earlier = previousGroups.assignments().find(item.first);
        if (earlier != previousGroups.assignments().end() &&
            !assignmentChanged(item.first, earlier->second)) continue;
        bindInt(db, addAssignment.get(), 1, item.first);
        bindText(db, addAssignment.get(), 2, item.second.groupId);
        bindText(db, addAssignment.get(), 3, item.second.studentName);
        writeRow(db, addAssignment.get());
    }
}


} // namespace

namespace SqliteStorage {

bool groupSubjectMarks(const std::string& path, const std::string& groupId,
                       std::vector<SubjectMarkRow>& output, std::string& error) {
    error.clear();
    try {
        auto db = open(path, false);
        ensureProjectDatabase(db.get(), false);
        execute(db.get(), "BEGIN");
        std::vector<SubjectMarkRow> result;
        try {
            verifyIntegrity(db.get());
            StudentManagement records;
            AcademicGroups groups;
            readSnapshot(db.get(), records, groups);
            const AcademicGroup* group = groups.findGroup(groupId);
            if (!group)
                throw std::runtime_error("Unknown academic group: " + groupId);

            // Every mark is identified by a subject code ONLY when its student
            // is assigned to this particular group. Same position across
            // unrelated groups does not imply the same subject.
            auto stmt = prepare(db.get(), R"SQL(
                SELECT s.roll_number, s.name, m.position,
                       subj.code, subj.name, m.mark
                FROM group_assignments AS a
                JOIN students AS s ON s.roll_number = a.roll_number
                JOIN marks AS m ON m.roll_number = s.roll_number
                JOIN subjects AS subj ON subj.group_id = a.group_id
                                     AND subj.position = m.position
                WHERE a.group_id = ?
                ORDER BY s.roll_number, m.position
            )SQL");
            bindText(db.get(), stmt.get(), 1, groupId);
            for (;;) {
                const int code = sqlite3_step(stmt.get());
                if (code == SQLITE_DONE) break;
                if (code != SQLITE_ROW) fail(db.get(), "Read named subject marks");
                result.push_back({sqlite3_column_int(stmt.get(), 0),
                                  columnText(stmt.get(), 1),
                                  sqlite3_column_int(stmt.get(), 2),
                                  columnText(stmt.get(), 3),
                                  columnText(stmt.get(), 4),
                                  sqlite3_column_int(stmt.get(), 5)});
            }
            std::size_t assignedCount = 0;
            for (const auto& assignment : groups.assignments())
                if (assignment.second.groupId == groupId) ++assignedCount;
            if (result.size() != assignedCount * group->subjects.size())
                throw std::runtime_error("Academic subjects and mark rows do not match");
            execute(db.get(), "COMMIT");
        } catch (...) {
            try { execute(db.get(), "ROLLBACK"); } catch (...) { }
            throw;
        }
        output = std::move(result);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

bool save(const std::string& path, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error) {
    error.clear();
    try {
        // This API is for one-time creation/import ONLY. Existing databases
        // must be edited through commit(expected, next) instead.
        requireNewDatabaseTarget(path);
        auto db = open(path, true);
        execute(db.get(), "PRAGMA synchronous = FULL");
        execute(db.get(), "BEGIN IMMEDIATE");
        try {
            // Check after taking the write lock too: simultaneous first-run
            // attempts must not overwrite whichever initializer committed first.
            ensureProjectDatabase(db.get(), true);
            if (version(db.get()) != 0)
                throw std::runtime_error("SQL database was initialized by another process; Reload");
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
            // Read one consistent SQLite snapshot without mutating outputs.
            verifyIntegrity(db.get());
            readSnapshot(db.get(), stagedRecords, stagedGroups);
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

bool matches(const std::string& path, const StudentManagement& expectedRecords,
             const AcademicGroups& expectedGroups, std::string& error) {
    StudentManagement actualRecords;
    AcademicGroups actualGroups;
    if (!load(path, actualRecords, actualGroups, error)) return false;
    if (!equalRecords(actualRecords, expectedRecords) ||
        !equalGroups(actualGroups, expectedGroups)) {
        error = "Database was modified by another process; Reload before editing";
        return false;
    }
    return true;
}

bool commit(const std::string& path, const StudentManagement& expectedRecords,
            const AcademicGroups& expectedGroups,
            const StudentManagement& nextRecords, const AcademicGroups& nextGroups,
            std::string& error) {
    error.clear();
    try {
        auto db = open(path, false, true);
        ensureProjectDatabase(db.get(), false);
        execute(db.get(), "PRAGMA synchronous = FULL");
        execute(db.get(), "BEGIN IMMEDIATE");
        try {
            StudentManagement persisted;
            AcademicGroups persistedGroups;
            verifyIntegrity(db.get());
            readSnapshot(db.get(), persisted, persistedGroups);
            if (!equalRecords(expectedRecords, persisted) ||
                !equalGroups(expectedGroups, persistedGroups))
                throw std::runtime_error("Database changed since Reload; no edits saved");
            writeDelta(db.get(), persisted, persistedGroups, nextRecords, nextGroups);
            execute(db.get(), "COMMIT");
        } catch (...) {
            try { execute(db.get(), "ROLLBACK"); } catch (...) { }
            throw;
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

// Read the exact source bytes; timestamps alone may miss a quick edit.
// Source-file size is bounded, so migration does not consume unbounded RAM.
static std::string legacyBytes(const std::string& path) {
    std::error_code ec;
    const auto bytes = fs::file_size(path, ec);
    if (ec || bytes > 16 * 1024 * 1024)
        throw std::runtime_error("Cannot read legacy source or it is too large: " + path);
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open legacy source: " + path);
    std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad() || result.size() != bytes)
        throw std::runtime_error("Legacy source changed during reading: " + path);
    return result;
}

bool importLegacy(const std::string& databaseFile, const std::string& studentsFile,
                  const std::string& groupsFile, std::string& error) {
    error.clear();
    const std::string staging = databaseFile + ".importing";
    bool ownsStaging = false, ownsFinal = false;
    const auto cleanup = [&]() {
        if (ownsStaging) {
            for (const auto& path : {staging, staging + "-wal", staging + "-shm",
                                     staging + "-journal"}) {
                std::error_code ignored;
                fs::remove(path, ignored);
            }
        }
    };
    try {
        // Check symlinks and sidecars, not only the main .db path.
        requireNewDatabaseTarget(databaseFile);
        requireNewDatabaseTarget(staging);
        ensureNoRecoveryFiles(studentsFile);
        ensureNoRecoveryFiles(groupsFile);
        std::error_code ec;
        const bool haveStudents = fs::exists(studentsFile, ec);
        if (ec) throw std::runtime_error("Cannot inspect student source: " + ec.message());
        if (!haveStudents) throw std::runtime_error("Legacy students file is missing");
        const bool haveGroups = fs::exists(groupsFile, ec);
        if (ec) throw std::runtime_error("Cannot inspect group source: " + ec.message());

        const std::string sourceStudents = legacyBytes(studentsFile);
        const std::string sourceGroups = haveGroups ? legacyBytes(groupsFile) : std::string();
        StudentManagement records;
        std::string readError;
        if (!RecordStorage::load(studentsFile, records, readError))
            throw std::runtime_error(readError);
        AcademicGroups groups;
        if (haveGroups && !AcademicGroupStorage::load(groupsFile, records, groups, readError))
            throw std::runtime_error(readError);
        if (!groups.consistentWith(records))
            throw std::runtime_error("Existing group assignments do not match students");
        const auto sourceUnchanged = [&]() {
            std::error_code check;
            const bool groupPresent = fs::exists(groupsFile, check);
            if (check || groupPresent != haveGroups) return false;
            return legacyBytes(studentsFile) == sourceStudents &&
                   (!haveGroups || legacyBytes(groupsFile) == sourceGroups);
        };
        if (!sourceUnchanged())
            throw std::runtime_error("Legacy source changed during migration; try again");

        // Build and fully verify a separate database. A failed import cannot
        // leave a half-initialized edutrack.db preventing another attempt.
        ownsStaging = true;
        std::string dbError;
        if (!save(staging, records, groups, dbError))
            throw std::runtime_error("Cannot stage imported database: " + dbError);
        StudentManagement checkedRecords;
        AcademicGroups checkedGroups;
        if (!load(staging, checkedRecords, checkedGroups, dbError))
            throw std::runtime_error("Cannot verify staged import: " + dbError);
        if (!equalRecords(records, checkedRecords) || !equalGroups(groups, checkedGroups))
            throw std::runtime_error("Staged SQL records differ from legacy data");
        if (!sourceUnchanged())
            throw std::runtime_error("Legacy source changed while SQL data was imported");

        // Publish only to a new filename. Hard links are no-clobber on local
        // filesystems; copy_file(none) is the no-overwrite portability fallback.
        requireNewDatabaseTarget(databaseFile);
        fs::create_hard_link(staging, databaseFile, ec);
        if (ec) {
            ec.clear();
            if (!fs::copy_file(staging, databaseFile, fs::copy_options::none, ec))
                throw std::runtime_error("Cannot publish imported database: " +
                                         (ec ? ec.message() : "destination exists"));
        }
        ownsFinal = true;
        StudentManagement finalRecords;
        AcademicGroups finalGroups;
        if (!load(databaseFile, finalRecords, finalGroups, dbError) ||
            !equalRecords(records, finalRecords) || !equalGroups(groups, finalGroups))
            throw std::runtime_error("Published SQL data failed verification: " + dbError);
        if (!sourceUnchanged())
            throw std::runtime_error("Legacy data changed before migration completed");
        cleanup();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        if (ownsFinal) {
            std::error_code ignored;
            fs::remove(databaseFile, ignored); // Only a file created by this importer.
        }
        cleanup();
        return false;
    }
}

} // namespace SqliteStorage
