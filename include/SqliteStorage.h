#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include "AcademicGroups.h"
#include "StudentManagement.h"

#include <string>
#include <vector>

// SQLite record and academic-group storage. Legacy text files are read only
// by the explicit, non-destructive migration tool.
namespace SqliteStorage {

// Subject codes are group-scoped. An unassigned student's positional marks
// deliberately have no subject code and do not appear in this result.
struct SubjectMarkRow {
    int rollNumber;
    std::string studentName;
    int position;
    std::string subjectCode;
    std::string subjectName;
    int mark;
};

// Read only marks that can be joined to named subjects in a verified group.
// A valid group with no assigned students produces an empty vector.
// On failure, output is unchanged.
bool groupSubjectMarks(const std::string& databaseFile, const std::string& groupId,
                       std::vector<SubjectMarkRow>& output, std::string& error);


// Creates a NEW SQLite database and atomically inserts both snapshots.
// Refuses any existing SQL database, dangling target symlink or WAL/journal
// sidecar. This function is ONLY for first-run initialization / migration;
// edit existing records with commit(expected, next) to prevent data loss.
bool save(const std::string& databaseFile, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error);

// Read the complete database into temporary objects. On any error, both output
// parameters remain unchanged. Does not create a missing database.
bool load(const std::string& databaseFile, StudentManagement& records,
          AcademicGroups& groups, std::string& error);

// Compare a stable, fully checked persisted snapshot with the caller's state.
// This is a convenience read; commit() repeats the check under a write lock.
bool matches(const std::string& databaseFile,
             const StudentManagement& expectedRecords, const AcademicGroups& expectedGroups,
             std::string& error);

// Optimistic, transactional write: compare both expected snapshots inside
// BEGIN IMMEDIATE, then update ONLY affected students/marks/groups/assignments.
// Concurrent changes are rejected; neither output nor old SQL data is changed
// on failure. The DB must already exist.
bool commit(const std::string& databaseFile,
            const StudentManagement& expectedRecords, const AcademicGroups& expectedGroups,
            const StudentManagement& newRecords, const AcademicGroups& newGroups,
            std::string& error);

// Import legacy students.txt + optional academic_groups.txt once. Refuses to
// overwrite an existing SQLite database, and never edits source text files.
bool importLegacy(const std::string& databaseFile,
                  const std::string& studentsFile,
                  const std::string& groupsFile, std::string& error);

} // namespace SqliteStorage
#endif
