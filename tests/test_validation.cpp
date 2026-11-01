#include "AcademicGroups.h"
#include "AcademicProcessing.h"
#include "GroupDefinitionInput.h"
#include "RecordAlgorithms.h"
#include "StudentManagement.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {
Student student(int roll, std::vector<int> marks, std::string name = "Demo Student",
                std::string branch = "CSE", int semester = 3) {
    return Student(roll, std::move(name), std::move(branch), semester, std::move(marks));
}

void checkStudentBoundaries() {
    const Student valid = student(1, {0, 100});
    assert(StudentManagement::isValid(valid));
    assert(StudentManagement::isValid(student(999999999, std::vector<int>(20, 100))));
    for (int roll : {-10, 0, 1000000000})
        assert(!StudentManagement::isValid(student(roll, {50})));
    assert(!StudentManagement::isValid(student(1, {})));
    assert(!StudentManagement::isValid(student(1, std::vector<int>(21, 50))));
    assert(!StudentManagement::isValid(student(1, {-1})));
    assert(!StudentManagement::isValid(student(1, {101})));
    assert(!StudentManagement::isValid(student(1, {50}, "   ")));
    assert(!StudentManagement::isValid(student(1, {50}, "A\nB")));
    assert(!StudentManagement::isValid(student(1, {50}, "A", "  ")));
    assert(!StudentManagement::isValid(student(1, {50}, "A", "CSE", 0)));
    assert(!StudentManagement::isValid(student(1, {50}, "A", "CSE", 13)));
    assert(!StudentManagement::isValid(student(1, {50}, std::string(81, 'A'))));
    assert(!StudentManagement::isValid(student(1, {50}, "A", std::string(61, 'B'))));

    StudentManagement records;
    assert(records.addStudent(valid));
    const auto before = records.allStudents().size();
    assert(!records.addStudent(valid));
    assert(!records.addStudent(student(2, {101})));
    assert(!records.updateStudent(1, student(2, {50})));
    assert(!records.updateStudent(1, student(1, {101})));
    assert(!records.deleteStudent(2));
    assert(records.allStudents().size() == before);
    assert(records.findStudent(1)->marks() == valid.marks());
}

void checkDeterministicRecordOperations() {
    StudentManagement records;
    std::vector<int> rolls(1500);
    std::iota(rolls.begin(), rolls.end(), 1);
    std::mt19937 random(20261101);
    std::shuffle(rolls.begin(), rolls.end(), random);
    for (int roll : rolls) {
        assert(records.addStudent(student(roll, {roll % 101, (roll * 3) % 101})));
    }
    assert(records.allStudents().size() == rolls.size());
    for (int i = 1; i <= 1500; ++i) {
        const Student* first = records.findStudent(i);
        const Student* binary = RecordAlgorithms::binarySearch(records.allStudents(), i);
        const Student* linear = RecordAlgorithms::linearSearch(records.allStudents(), i);
        assert(first && first == binary && first == linear);
        assert(records.allStudents()[static_cast<std::size_t>(i - 1)].rollNumber() == i);
    }
    std::shuffle(rolls.begin(), rolls.end(), random);
    for (int index = 0; index < 1500; ++index) {
        if (index % 2 == 0) {
            assert(records.deleteStudent(rolls[static_cast<std::size_t>(index)]));
        } else {
            const int roll = rolls[static_cast<std::size_t>(index)];
            assert(records.updateStudent(roll, student(roll, {100, 0})));
        }
    }
    assert(records.allStudents().size() == 750);
    const auto view = RecordAlgorithms::sortByPercentage(records.allStudents());
    assert(view.size() == 750);
    assert(std::all_of(view.begin(), view.end(), [](const Student* p) {
        return std::fabs(RecordAlgorithms::percentage(*p) - 50.0) < 0.000001;
    }));
    const auto ranks = RecordAlgorithms::rankList(records.allStudents());
    assert(ranks.size() == 750);
    assert(std::all_of(ranks.begin(), ranks.end(), [](const RecordAlgorithms::RankedStudent& item) {
        return item.rank == 1;
    }));
}

void checkGroupValidationAndAssignments() {
    AcademicGroups groups;
    const AcademicGroup valid{"CS2025", "CSE", 3, "2025",
                              {{"DS", "Data Structures"}, {"MA", "Mathematics"}}};
    assert(groups.addGroup(valid));
    assert(!groups.addGroup(valid));
    auto altered = valid;
    altered.id = "bad.id";
    assert(!groups.addGroup(altered));
    altered = valid; altered.id = "NEW"; altered.semester = 0;
    assert(!groups.addGroup(altered));
    altered.semester = 13;
    assert(!groups.addGroup(altered));
    altered = valid; altered.id = "NEW"; altered.cohort = "1999";
    assert(!groups.addGroup(altered));
    altered.cohort = "2101";
    assert(!groups.addGroup(altered));
    altered = valid; altered.id = "NEW"; altered.subjects[1].code = "DS";
    assert(!groups.addGroup(altered));
    altered = valid; altered.id = "NEW"; altered.subjects[0].code = "data";
    assert(!groups.addGroup(altered));
    altered = valid; altered.id = "NEW"; altered.subjects.resize(21, {"XX", "Extra"});
    assert(!groups.addGroup(altered));

    StudentManagement records;
    assert(records.addStudent(student(11, {60, 80})));
    assert(records.addStudent(student(12, {60, 80}, "Other", "ECE")));
    assert(records.addStudent(student(13, {60}, "Other")));
    assert(!groups.assign(*records.findStudent(12), "CS2025"));
    assert(!groups.assign(*records.findStudent(13), "CS2025"));
    assert(!groups.assign(*records.findStudent(11), "MISSING"));
    assert(groups.assign(*records.findStudent(11), "CS2025"));
    assert(groups.consistentWith(records));
    assert(!groups.comparable(*records.findStudent(11), *records.findStudent(12)));
    assert(records.updateStudent(11, student(11, {60, 80}, "Different name")));
    assert(!groups.consistentWith(records));
    assert(!groups.groupFor(*records.findStudent(11)));
    assert(records.updateStudent(11, student(11, {60, 80})));
    assert(groups.consistentWith(records));
    assert(groups.unassign(11));
    assert(!groups.unassign(11));
    assert(groups.consistentWith(records));

    std::vector<SubjectDefinition> parsed{{"OLD", "Keep"}};
    for (const char* invalid : {"", "DS Data Structures", "DS |", "DS | Data\n\nMA | Maths", "DS | Data | Bad"}) {
        assert(!GroupDefinitionInput::parseSubjects(invalid, parsed));
        assert(parsed.size() == 1 && parsed[0].code == "OLD");
    }
    assert(GroupDefinitionInput::parseSubjects("DS | Data Structures\r\nMA | Maths\n", parsed));
    assert(parsed.size() == 2 && parsed[1].code == "MA");
}

void checkGradesAndEmptyStates() {
    using AcademicProcessing::GradeScale;
    assert(AcademicProcessing::grade(85.0, GradeScale::GeuReference) == "A+");
    assert(AcademicProcessing::grade(85.001, GradeScale::GeuReference) == "O");
    assert(AcademicProcessing::grade(40.0, GradeScale::GeuReference) == "P");
    assert(AcademicProcessing::grade(39.999, GradeScale::GeuReference) == "F");
    assert(AcademicProcessing::grade(90.0, GradeScale::OriginalProject) == "A");
    assert(AcademicProcessing::grade(59.0, GradeScale::OriginalProject) == "F");
    assert(!AcademicProcessing::classAverage({}));
    assert(AcademicProcessing::topStudent({}) == nullptr);
    assert(RecordAlgorithms::rankList({}).empty());
}
}  // namespace

int main() {
    checkStudentBoundaries();
    checkDeterministicRecordOperations();
    checkGroupValidationAndAssignments();
    checkGradesAndEmptyStates();
    std::cout << "Boundary validation and deterministic record checks passed.\n";
}