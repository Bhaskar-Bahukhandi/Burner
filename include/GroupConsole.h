#ifndef GROUP_CONSOLE_H
#define GROUP_CONSOLE_H

#include "AcademicGroupStorage.h"
#include "GroupRankSimulation.h"

#include <iosfwd>
#include <string>

namespace GroupConsole {
// Choices 21-25. Metadata changes save immediately, only on success.
// False means input ended; true means the command was handled.
bool handle(int choice, const StudentManagement& records, AcademicGroups& groups,
            const std::string& filename, AcademicProcessing::GradeScale scale,
            std::istream& input, std::ostream& output);
}
#endif
