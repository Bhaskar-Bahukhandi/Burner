#include "AcademicGroups.h"

#include <cctype>
#include <set>

namespace {
bool validCode(const std::string& code, std::size_t limit) {
    if (code.empty() || code.size() > limit) return false;
    for (unsigned char c : code) {
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) return false;
    }
    return true;
}

bool validText(const std::string& text, std::size_t limit) {
    if (text.empty() || text.size() > limit) return false;
    bool nonspace = false;
    for (unsigned char c : text) {
        if (std::iscntrl(c)) return false;
        if (!std::isspace(c)) nonspace = true;
    }
    return nonspace;
}

bool validGroup(const AcademicGroup& group) {
    if (!validCode(group.id, 32) || !validText(group.branch, 60) ||
        group.semester < 1 || group.semester > 12 ||
        group.cohort.size() != 4 || group.subjects.empty() ||
        group.subjects.size() > 20) return false;
    for (unsigned char c : group.cohort) {
        if (c < '0' || c > '9') return false;
    }
    if (group.cohort < "2000" || group.cohort > "2100") return false;
    std::set<std::string> codes;
    for (const SubjectDefinition& subject : group.subjects) {
        if (!validCode(subject.code, 24) || !validText(subject.name, 80) ||
            !codes.insert(subject.code).second) return false;
    }
    return true;
}
}  // namespace

bool AcademicGroups::addGroup(const AcademicGroup& group) {
    if (!validGroup(group) || groups_.size() >= 200) return false;
    return groups_.emplace(group.id, group).second;
}

const AcademicGroup* AcademicGroups::findGroup(const std::string& groupId) const {
    auto it = groups_.find(groupId);
    return it == groups_.end() ? nullptr : &it->second;
}

bool AcademicGroups::assign(const Student& student, const std::string& groupId) {
    const AcademicGroup* group = findGroup(groupId);
    if (!group || student.rollNumber() <= 0 || student.rollNumber() > 999999999 ||
        student.branch() != group->branch || student.semester() != group->semester ||
        student.marks().size() != group->subjects.size() ||
        (assignments_.find(student.rollNumber()) == assignments_.end() &&
         assignments_.size() >= 10000)) return false;
    for (int mark : student.marks()) {
        if (mark < 0 || mark > 100) return false;
    }
    assignments_[student.rollNumber()] = {groupId, student.name()};
    return true;
}

bool AcademicGroups::unassign(int rollNumber) {
    return assignments_.erase(rollNumber) != 0;
}

const AcademicGroup* AcademicGroups::groupFor(const Student& student) const {
    const auto assigned = assignments_.find(student.rollNumber());
    if (assigned == assignments_.end()) return nullptr;
    if (assigned->second.studentName != student.name()) return nullptr;
    const AcademicGroup* group = findGroup(assigned->second.groupId);
    if (!group || group->branch != student.branch() ||
        group->semester != student.semester() ||
        group->subjects.size() != student.marks().size()) return nullptr;
    for (int mark : student.marks()) {
        if (mark < 0 || mark > 100) return nullptr;
    }
    return group;
}

bool AcademicGroups::comparable(const Student& first, const Student& second) const {
    if (first.rollNumber() == second.rollNumber()) return false;
    const AcademicGroup* left = groupFor(first);
    const AcademicGroup* right = groupFor(second);
    return left && right && left->id == right->id;
}

bool AcademicGroups::consistentWith(const StudentManagement& records) const {
    for (const auto& assignment : assignments_) {
        const Student* student = records.findStudent(assignment.first);
        if (!student || !groupFor(*student)) return false;
    }
    return true;
}

const std::map<std::string, AcademicGroup>& AcademicGroups::groups() const {
    return groups_;
}

const std::map<int, GroupAssignment>& AcademicGroups::assignments() const {
    return assignments_;
}
