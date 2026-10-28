#include "DatabaseMaintenance.h"
#include "SqliteStorage.h"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

struct DatabaseCloser {
    void operator()(sqlite3* db) const { if (db) sqlite3_close(db); }
};
using Database = std::unique_ptr<sqlite3, DatabaseCloser>;
struct StatementCloser {
    void operator()(sqlite3_stmt* stmt) const { if (stmt) sqlite3_finalize(stmt); }
};
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;

Database openExisting(const std::string& path) {
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        std::string message = raw ? sqlite3_errmsg(raw) : "SQLite allocation failed";
        if (raw) sqlite3_close(raw);
        throw std::runtime_error("Cannot open source database: " + message);
    }
    Database db(raw);
    sqlite3_busy_timeout(db.get(), 5000);
    return db;
}

Statement prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
    return Statement(raw);
}

void verifySqliteFile(const std::string& path) {
    auto db = openExisting(path);
    auto check = prepare(db.get(), "PRAGMA integrity_check");
    if (sqlite3_step(check.get()) != SQLITE_ROW ||
        sqlite3_column_type(check.get(), 0) != SQLITE_TEXT ||
        std::string(reinterpret_cast<const char*>(sqlite3_column_text(check.get(), 0))) != "ok" ||
        sqlite3_step(check.get()) != SQLITE_DONE)
        throw std::runtime_error("Full SQLite integrity_check failed");

    auto fk = prepare(db.get(), "PRAGMA foreign_key_check");
    const int foreignResult = sqlite3_step(fk.get());
    if (foreignResult != SQLITE_DONE) {
        if (foreignResult == SQLITE_ROW)
            throw std::runtime_error("Foreign-key violations found in database");
        throw std::runtime_error("Cannot complete foreign-key check");
    }
    // Verify that this is the supported EduTrack schema and that every record,
    // subject and group assignment satisfies the application data model.
    StudentManagement students;
    AcademicGroups groups;
    std::string error;
    if (!SqliteStorage::load(path, students, groups, error))
        throw std::runtime_error("EduTrack records failed validation: " + error);
}

bool existsChecked(const fs::path& path) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    // An existing symlink (even a dangling one) must never be overwritten.
    if (ec && ec != std::errc::no_such_file_or_directory)
        throw std::runtime_error("Cannot inspect destination: " + ec.message());
    return status.type() != fs::file_type::not_found;
}

void requireDistinct(const fs::path& source, const fs::path& destination) {
    if (source.empty() || destination.empty())
        throw std::runtime_error("Database paths cannot be empty");
    std::error_code ec;
    const fs::path canonicalSource = fs::weakly_canonical(source, ec);
    if (ec) throw std::runtime_error("Cannot resolve source path: " + ec.message());
    const fs::path canonicalDest = fs::weakly_canonical(destination, ec);
    if (ec) throw std::runtime_error("Cannot resolve destination path: " + ec.message());
    if (canonicalSource == canonicalDest)
        throw std::runtime_error("Backup source and destination must be different files");
}

// A staged file is never silently removed if it existed before this call.
// All staging files must be new; a leftover one requires human inspection.
void requireAvailable(const fs::path& destination, const fs::path& staged) {
    for (const auto& path : {destination, staged,
                             fs::path(staged.string() + "-wal"),
                             fs::path(staged.string() + "-shm"),
                             fs::path(staged.string() + "-journal")}) {
        if (existsChecked(path))
            throw std::runtime_error("Destination/staging file already exists: " + path.string());
    }
    if (!fs::exists(destination.parent_path().empty() ? fs::path(".") : destination.parent_path()))
        throw std::runtime_error("Destination directory does not exist");
}

void copyUsingSqlite(const std::string& source, const std::string& staged) {
    auto src = openExisting(source);
#ifndef _WIN32
    // Create the staging file with owner-only permissions *before* any rows
    // are copied. O_EXCL also protects against a raced symlink/staging file.
    const int fd = ::open(staged.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1)
        throw std::runtime_error("Cannot securely create backup staging: " +
                                 std::string(std::strerror(errno)));
    ::close(fd);
#endif
    sqlite3* raw = nullptr;
    const int openCode = sqlite3_open_v2(staged.c_str(), &raw,
                                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (openCode != SQLITE_OK) {
        const std::string message = raw ? sqlite3_errmsg(raw) : "SQLite allocation failed";
        if (raw) sqlite3_close(raw);
        throw std::runtime_error("Cannot create backup staging database: " + message);
    }
    Database dest(raw);
    sqlite3_busy_timeout(dest.get(), 5000);
    sqlite3_backup* task = sqlite3_backup_init(dest.get(), "main", src.get(), "main");
    if (!task)
        throw std::runtime_error("Cannot start online SQLite backup: " +
                                 std::string(sqlite3_errmsg(dest.get())));
    const int stepResult = sqlite3_backup_step(task, -1);
    const int finishResult = sqlite3_backup_finish(task);
    if (stepResult != SQLITE_DONE || finishResult != SQLITE_OK)
        throw std::runtime_error("Online SQLite backup failed: " +
                                 std::string(sqlite3_errmsg(dest.get())));
    // Flush staged file through SQLite before checking/publishing it.
}

// Atomic creation on filesystems supporting hard links. The copy fallback
// also refuses to overwrite an existing destination; partial copies are
// removed on failure and a final full validation is required.
void publish(const fs::path& staged, const fs::path& destination) {
    std::error_code ec;
    fs::create_hard_link(staged, destination, ec);
    if (ec) {
        ec.clear();
        if (!fs::copy_file(staged, destination, fs::copy_options::none, ec)) {
            if (ec) throw std::runtime_error("Cannot publish verified database: " + ec.message());
            throw std::runtime_error("Destination already exists; not overwriting it");
        }
    }
}

void cleanOwnedStaging(const fs::path& staged) {
    // sqlite3_backup uses the destination main database, but cleanup known
    // SQLite sidecars as well if a failed copy created them.
    for (const auto& item : {staged,
                             fs::path(staged.string() + "-wal"),
                             fs::path(staged.string() + "-shm"),
                             fs::path(staged.string() + "-journal")}) {
        std::error_code ignored;
        fs::remove(item, ignored);
    }
}

bool copyVerified(const std::string& sourcePath, const std::string& targetPath,
                  std::string& error) {
    error.clear();
    fs::path staged;
    bool ownsStaging = false;
    bool ownsFinal = false;
    try {
        const fs::path source(sourcePath), target(targetPath);
        requireDistinct(source, target);
        // Reject a source that isn't a readable, valid EduTrack database.
        verifySqliteFile(sourcePath);
        staged = fs::path(target.string() + ".pending");
        requireAvailable(target, staged);
        ownsStaging = true;
        copyUsingSqlite(sourcePath, staged.string());
        verifySqliteFile(staged.string());
#ifndef _WIN32
        // This controls the new DB's Unix mode, not parent-directory rights,
        // past backups, disk snapshots or any encryption.
        fs::permissions(staged, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace);
#endif
        publish(staged, target);
        ownsFinal = true;
        verifySqliteFile(targetPath);
        cleanOwnedStaging(staged);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        if (ownsFinal) {
            // This file was created by this function, never an existing DB.
            std::error_code ignored;
            fs::remove(fs::path(targetPath), ignored);
        }
        if (ownsStaging) cleanOwnedStaging(staged);
        return false;
    }
}

} // namespace

namespace DatabaseMaintenance {

bool verify(const std::string& databaseFile, std::string& error) {
    error.clear();
    try {
        verifySqliteFile(databaseFile);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

bool backup(const std::string& databaseFile, const std::string& backupFile,
            std::string& error) {
    return copyVerified(databaseFile, backupFile, error);
}

bool restoreToNewFile(const std::string& backupFile, const std::string& newFile,
                      std::string& error) {
    return copyVerified(backupFile, newFile, error);
}

} // namespace DatabaseMaintenance
