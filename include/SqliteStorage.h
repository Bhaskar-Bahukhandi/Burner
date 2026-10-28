#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include "AcademicGroups.h"
#include "StudentManagement.h"

#include <string>

// Stage 3G2A: SQLite storage foundation. The application is not switched to
// this backend yet. Existing EDUTRACK 1 and GROUPS 1 files are never edited.
namespace SqliteStorage {

// Replaces both snapshots in a single SQLite transaction. On an error, the
// previous committed database contents are preserved. Source objects are not
// changed. This initial API is deliberately simple; a later integration will
// replace full-snapshot saves with targeted SQL operations.
bool save(const std::string& databaseFile, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error);

// Read the complete database into temporary objects. On any error, both output
// parameters remain unchanged. Does not create a missing database.
bool load(const std::string& databaseFile, StudentManagement& records,
          AcademicGroups& groups, std::string& error);

// Import legacy students.txt + optional academic_groups.txt once. Refuses to
// overwrite an existing SQLite database, and never edits source text files.
bool importLegacy(const std::string& databaseFile,
                  const std::string& studentsFile,
                  const std::string& groupsFile, std::string& error);

} // namespace SqliteStorage
#endif
