#ifndef RECORD_STORAGE_H
#define RECORD_STORAGE_H

#include "StudentManagement.h"

#include <string>

namespace RecordStorage {

// On failure, the existing records and existing saved file are left alone.
// The error message explains how to recover if the backup could not be restored.
bool save(const std::string& filename, const StudentManagement& records,
          std::string& error);
bool load(const std::string& filename, StudentManagement& records,
          std::string& error);

}  // namespace RecordStorage

#endif
