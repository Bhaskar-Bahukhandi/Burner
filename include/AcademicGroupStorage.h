#ifndef ACADEMIC_GROUP_STORAGE_H
#define ACADEMIC_GROUP_STORAGE_H

#include "AcademicGroups.h"

#include <string>

namespace AcademicGroupStorage {

// Sidecar file. The original students.txt (EDUTRACK 1) format stays intact.
// On failure, load leaves existing groups unchanged; save never edits students.txt.
bool load(const std::string& filename, const StudentManagement& records,
          AcademicGroups& groups, std::string& error);
bool save(const std::string& filename, const StudentManagement& records,
          const AcademicGroups& groups, std::string& error);

}  // namespace AcademicGroupStorage

#endif
