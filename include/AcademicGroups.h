#ifndef ACADEMIC_GROUPS_H
#define ACADEMIC_GROUPS_H

#include "StudentManagement.h"

#include <map>
#include <string>
#include <vector>

struct SubjectDefinition {
    std::string code;
    std::string name;
};

struct AcademicGroup {
    std::string id;
    std::string branch;
    int semester;
    std::string cohort; // e.g. 2025; explicitly entered, never inferred
    std::vector<SubjectDefinition> subjects; // subject order matches marks order
};

struct GroupAssignment {
    std::string groupId;
    std::string studentName; // checks roll-number reuse and changed identity
};

class AcademicGroups {
public:
    bool addGroup(const AcademicGroup& group);
    bool assign(const Student& student, const std::string& groupId);
    bool unassign(int rollNumber);

    const AcademicGroup* findGroup(const std::string& groupId) const;
    const AcademicGroup* groupFor(const Student& student) const;
    bool comparable(const Student& first, const Student& second) const;
    bool consistentWith(const StudentManagement& records) const;

    const std::map<std::string, AcademicGroup>& groups() const;
    const std::map<int, GroupAssignment>& assignments() const;

private:
    std::map<std::string, AcademicGroup> groups_;
    std::map<int, GroupAssignment> assignments_;
};

#endif
