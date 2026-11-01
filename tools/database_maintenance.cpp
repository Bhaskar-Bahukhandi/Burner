#include "DatabaseMaintenance.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "verify") {
        std::string error;
        if (!DatabaseMaintenance::verify(argv[2], error)) {
            std::cerr << "Verification failed: " << error << '\n';
            return 1;
        }
        std::cout << "Verified SQLite integrity, foreign keys and EduTrack records.\n";
        return 0;
    }
    if (argc == 4) {
        const std::string action = argv[1];
        std::string error;
        const bool ok = action == "backup"
            ? DatabaseMaintenance::backup(argv[2], argv[3], error)
            : action == "restore"
                ? DatabaseMaintenance::restoreToNewFile(argv[2], argv[3], error)
                : false;
        if (action == "backup" || action == "restore") {
            if (!ok) {
                std::cerr << action << " failed: " << error << '\n';
                return 1;
            }
            std::cout << "Created and verified new database: " << argv[3] << '\n';
            if (action == "restore")
                std::cout << "The live edutrack.db was NOT replaced. Close all app windows, "
                             "keep a copy of the old database and activate the restored "
                             "file manually after verification.\n";
            return 0;
        }
    }
    std::cerr << "Usage:\n"
              << "  edutrack-db verify <database.db>\n"
              << "  edutrack-db backup <edutrack.db> <new-backup.db>\n"
              << "  edutrack-db restore <verified-backup.db> <new-restored.db>\n"
              << "Never use the live DB as the restore destination.\n";
    return 2;
}
