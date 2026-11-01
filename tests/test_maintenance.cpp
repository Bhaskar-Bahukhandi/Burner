#include "DatabaseMaintenance.h"
#include "SqliteStorage.h"

#include <sqlite3.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path dir = fs::temp_directory_path() /
                         ("edutrack_backup_check_" + std::to_string(now));
    fs::create_directory(dir);
    const auto source = (dir / "source.db").string();
    const auto backup = (dir / "backup.db").string();
    const auto restored = (dir / "restored.db").string();
    std::string error;
    StudentManagement students;
    AcademicGroups groups;
    assert(students.addStudent(Student(7, "Asha", "CSE", 3, {75, 80})));
    const AcademicGroup csGroup{"CS25", "CSE", 3, "2025",
                                {{"DS", "Data Structures"}, {"MA", "Maths"}}};
    assert(groups.addGroup(csGroup));
    assert(groups.assign(*students.findStudent(7), "CS25"));
    assert(SqliteStorage::save(source, students, groups, error));

    sqlite3* connection = nullptr;
    assert(sqlite3_open_v2(source.c_str(), &connection, SQLITE_OPEN_READWRITE, nullptr)
           == SQLITE_OK);
    assert(sqlite3_exec(connection, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr)
           == SQLITE_OK);
    assert(sqlite3_exec(connection, "UPDATE marks SET mark=86 WHERE roll_number=7 AND position=1",
                        nullptr, nullptr, nullptr) == SQLITE_OK);
    // Keep the connection open to ensure backup sees committed WAL content.
    assert(DatabaseMaintenance::backup(source, backup, error));
    assert(DatabaseMaintenance::verify(backup, error));
    StudentManagement loaded;
    AcademicGroups loadedGroups;
    assert(SqliteStorage::load(backup, loaded, loadedGroups, error));
    assert(loaded.findStudent(7)->marks()[0] == 86);
    assert(loadedGroups.groupFor(*loaded.findStudent(7)));
    assert(!DatabaseMaintenance::backup(source, backup, error));
    assert(DatabaseMaintenance::restoreToNewFile(backup, restored, error));
    assert(SqliteStorage::load(restored, loaded, loadedGroups, error));
    assert(loaded.findStudent(7)->marks()[0] == 86);
    assert(!DatabaseMaintenance::restoreToNewFile(backup, restored, error));

    // A database destination without its main file but with a stale WAL
    // cannot be used for a new backup.
    const auto blocked = (dir / "blocked.db").string();
    { std::ofstream file(blocked + "-wal"); file << "leftover"; }
    assert(!DatabaseMaintenance::backup(source, blocked, error));
    assert(!fs::exists(blocked));
    const auto damaged = (dir / "bad.db").string();
    { std::ofstream file(damaged, std::ios::binary); file << "not sqlite"; }
    assert(!DatabaseMaintenance::verify(damaged, error));
    assert(!DatabaseMaintenance::restoreToNewFile(damaged, (dir / "new.db").string(), error));
    assert(!fs::exists(dir / "new.db"));
    assert(sqlite3_close(connection) == SQLITE_OK);
    fs::remove_all(dir);
    std::cout << "SQLite WAL backup, restore and destination safety checks passed.\n";
}
