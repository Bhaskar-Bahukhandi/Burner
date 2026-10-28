#include "SqliteStorage.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: edutrack-migrate <students.txt> <academic_groups.txt> <edutrack.db>\n"
                  << "The original text files are preserved. The database must not exist.\n";
        return 2;
    }
    std::string error;
    if (!SqliteStorage::importLegacy(argv[3], argv[1], argv[2], error)) {
        std::cerr << "Migration refused: " << error << '\n';
        return 1;
    }
    std::cout << "Imported and verified snapshot in " << argv[3] << '\n'
              << "Old text files remain unchanged. Do not delete them.\n"
              << "Note: The console and GUIs still use text files until SQL integration.\n";
    return 0;
}
