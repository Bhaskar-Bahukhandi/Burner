#ifndef DATABASE_MAINTENANCE_H
#define DATABASE_MAINTENANCE_H

#include <string>

namespace DatabaseMaintenance {

// Runs a complete SQLite integrity check, checks relational constraints and
// verifies that EduTrack can load every student, mark, subject and assignment.
// Does not modify the database or create a missing database.
bool verify(const std::string& databaseFile, std::string& error);

// Creates a consistent online backup using sqlite3_backup, even if the source
// currently has committed WAL pages. Does NOT overwrite an existing file.
// The new copy is fully checked before being published at backupFile.
bool backup(const std::string& databaseFile, const std::string& backupFile,
            std::string& error);

// Recovers a *verified* backup into a NEW database filename. Refuses to
// overwrite an existing DB, including the active edutrack.db. To activate a
// restored database, close all EduTrack programs first and manually switch
// files after separately preserving the old DB and any sidecar files.
bool restoreToNewFile(const std::string& backupFile, const std::string& newFile,
                      std::string& error);

} // namespace DatabaseMaintenance

#endif
