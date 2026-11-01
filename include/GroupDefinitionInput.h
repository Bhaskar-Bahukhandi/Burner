#ifndef EDUTRACK_GROUP_DEFINITION_INPUT_H
#define EDUTRACK_GROUP_DEFINITION_INPUT_H

#include "AcademicGroups.h"

#include <sstream>
#include <string>
#include <vector>
#include <utility>

// A group definition uses one subject per line: CODE | Subject name.
// Parsing is separate from the Win32 form so the input rules can be tested.
namespace GroupDefinitionInput {
inline std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
inline bool parseSubjects(const std::string& lines, std::vector<SubjectDefinition>& result) {
    std::vector<SubjectDefinition> parsed;
    std::istringstream input(lines);
    std::string line;
    while (std::getline(input, line)) {
        // A single final newline is fine; empty rows between subjects are not.
        if (trim(line).empty()) return false;
        const auto bar = line.find('|');
        if (bar == std::string::npos || line.find('|', bar + 1) != std::string::npos)
            return false;
        std::string code = trim(line.substr(0, bar));
        std::string name = trim(line.substr(bar + 1));
        if (code.empty() || name.empty() || parsed.size() >= 20) return false;
        parsed.push_back({std::move(code), std::move(name)});
    }
    if (parsed.empty()) return false;
    result = std::move(parsed);
    return true;
}
} // namespace GroupDefinitionInput
#endif
