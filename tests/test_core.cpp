#include "AcademicGroups.h"
#include "AcademicProcessing.h"
#include "GroupRankSimulation.h"
#include "GroupDefinitionInput.h"
#include "ImprovementGoalPlanner.h"
#include "RecordAlgorithms.h"
#include "StudentManagement.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace {
Student makeStudent(int roll, std::vector<int> marks, const std::string& branch = "CSE") {
    return Student(roll, "Student " + std::to_string(roll), branch, 3, std::move(marks));
}

void checkSearchAndSort() {
    StudentManagement records;
    assert(!records.findStudent(10));
    assert(!records.deleteStudent(10));
    assert(records.addStudent(makeStudent(30, {70, 80})));
    assert(records.addStudent(makeStudent(10, {60, 90})));
    assert(records.addStudent(makeStudent(20, {0, 100})));
    assert(!records.addStudent(makeStudent(10, {99, 99})));
    assert(records.allStudents()[0].rollNumber() == 10);
    assert(records.allStudents()[1].rollNumber() == 20);
    assert(records.findStudent(20)->marks()[0] == 0);
    assert(!RecordAlgorithms::binarySearch(records.allStudents(), 21));
    assert(records.updateStudent(20, makeStudent(20, {100, 100})));
    assert(!records.updateStudent(20, makeStudent(22, {100})));
    assert(!records.updateStudent(20, makeStudent(20, {101})));
    assert(records.deleteStudent(10));
    assert(!records.findStudent(10));
    assert(records.findStudent(20)->marks()[0] == 100);
    assert(records.addStudent(makeStudent(10, {95, 95})));
    assert(records.allStudents().front().rollNumber() == 10);

    // Repeated lookups use the already sorted records; no per-search sorting.
    StudentManagement large;
    std::vector<int> rolls(10000);
    std::iota(rolls.begin(), rolls.end(), 1);
    std::mt19937 rng(42);
    std::shuffle(rolls.begin(), rolls.end(), rng);
    for (int roll : rolls) assert(large.addStudent(makeStudent(roll, {roll % 101})));
    for (int roll = 1; roll <= 10000; ++roll) {
        const Student* binary = RecordAlgorithms::binarySearch(large.allStudents(), roll);
        assert(binary && binary->rollNumber() == roll);
        assert(large.findStudent(roll) == binary);
    }
    assert(!large.findStudent(10001));
    assert(!large.findStudent(-1));
    const auto byRoll = RecordAlgorithms::sortByRollNumber(large.allStudents());
    assert(byRoll.front()->rollNumber() == 1);
    assert(byRoll.back()->rollNumber() == 10000);
    assert(large.deleteStudent(5000));
    assert(!large.findStudent(5000));
    assert(large.addStudent(makeStudent(5000, {0})));
    assert(large.findStudent(5000));
}

void checkRanking() {
    StudentManagement records;
    assert(records.addStudent(makeStudent(1, {80, 80})));
    assert(records.addStudent(makeStudent(2, {70, 90})));
    assert(records.addStudent(makeStudent(3, {90, 70, 80})));
    assert(records.addStudent(makeStudent(4, {75, 75})));
    const auto rank = RecordAlgorithms::rankList(records.allStudents());
    assert(rank.size() == 4);
    assert(rank[0].rank == 1 && rank[1].rank == 1 && rank[2].rank == 1);
    assert(rank[3].rank == 4);
    for (int roll = 1; roll <= 3; ++roll)
        assert(RecordAlgorithms::rankOf(records.allStudents(), roll) == 1);
    assert(RecordAlgorithms::rankOf(records.allStudents(), 4) == 4);
    assert(!RecordAlgorithms::rankOf(records.allStudents(), 999));
    assert(RecordAlgorithms::rankList({}).empty());
    const auto totalSorted = RecordAlgorithms::sortByTotalMarks(records.allStudents());
    assert(totalSorted.front()->rollNumber() == 3);
}

void checkGroupReportsAndSimulation() {
    StudentManagement students;
    for (const auto& student : std::vector<Student>{
        makeStudent(1, {80,80}), makeStudent(2, {70,90}),
        makeStudent(3, {90,70}), makeStudent(4, {75,75}),
        makeStudent(5, {10,100}, "ECE"), makeStudent(6, {100,100})})
        assert(students.addStudent(student));
    AcademicGroups groups;
    const AcademicGroup cseGroup{"CSE25", "CSE", 3, "2025",
                                 {{"DS", "Data Structures"}, {"MA", "Maths"}}};
    const AcademicGroup eceGroup{"ECE25", "ECE", 3, "2025",
                                 {{"EC", "Circuits"}, {"DI", "Digital"}}};
    const AcademicGroup emptyGroup{"EMPTY", "CSE", 3, "2026",
                                   {{"DS", "Data Structures"}, {"MA", "Maths"}}};
    assert(groups.addGroup(cseGroup));
    assert(groups.addGroup(eceGroup));
    assert(groups.addGroup(emptyGroup));
    for (int i = 1; i <= 4; ++i) assert(groups.assign(*students.findStudent(i), "CSE25"));
    assert(groups.assign(*students.findStudent(5), "ECE25"));
    const auto cse = AcademicProcessing::groupSubjectResults(students, groups, "CSE25");
    assert(cse && cse->size() == 2);
    assert((*cse)[0].code == "DS" && (*cse)[0].name == "Data Structures");
    assert((*cse)[0].students == 4);
    assert(std::fabs((*cse)[0].average - 78.75) < 1e-9);
    assert((*cse)[0].lowest == 70 && (*cse)[0].highest == 90);
    const auto ece = AcademicProcessing::groupSubjectResults(students, groups, "ECE25");
    assert(ece && ece->size() == 2 && (*ece)[0].average == 10);
    const auto empty = AcademicProcessing::groupSubjectResults(students, groups, "EMPTY");
    assert(empty && empty->empty());
    assert(!AcademicProcessing::groupSubjectResults(students, groups, "UNKNOWN"));

    const auto simulated = GroupRankSimulation::calculate(
        students, groups, 1, 1, 60, AcademicProcessing::GradeScale::GeuReference);
    assert(simulated && simulated->currentRank == 1 && simulated->proposedRank == 4);
    assert(students.findStudent(1)->marks()[0] == 80);
    assert(groups.unassign(1));
    assert(groups.consistentWith(students));
    assert(!AcademicProcessing::groupSubjectResults(students, groups, "CSE25")->empty());
}

void checkGroupInput() {
    std::vector<SubjectDefinition> parsed{{"PREVIOUS", "Kept on failure"}};
    assert(!GroupDefinitionInput::parseSubjects("", parsed));
    assert(parsed.size() == 1 && parsed[0].code == "PREVIOUS");
    assert(!GroupDefinitionInput::parseSubjects("DS Data Structures", parsed));
    assert(!GroupDefinitionInput::parseSubjects("DS |\n", parsed));
    assert(!GroupDefinitionInput::parseSubjects("DS | Data\n\nMATH | Maths", parsed));
    assert(!GroupDefinitionInput::parseSubjects("DS | Data | Structures", parsed));
    assert(GroupDefinitionInput::parseSubjects(
        " DS | Data Structures\r\n MATH | Mathematics\r\n", parsed));
    assert(parsed.size() == 2 && parsed[0].code == "DS" && parsed[0].name == "Data Structures");
    assert(parsed[1].code == "MATH" && parsed[1].name == "Mathematics");
    AcademicGroups groups;
    const AcademicGroup validGroup{"CS25", "CSE", 3, "2025", parsed};
    const AcademicGroup duplicateSubjects{"BAD", "CSE", 3, "2025",
                                          {{"DS", "A"}, {"DS", "B"}}};
    const AcademicGroup invalidSubjectCode{"BAD", "CSE", 3, "2025",
                                           {{"lowercase", "A"}}};
    assert(groups.addGroup(validGroup));
    assert(!groups.addGroup(validGroup));
    assert(!groups.addGroup(duplicateSubjects));
    assert(!groups.addGroup(invalidSubjectCode));
    std::string overlong;
    for (int i = 1; i <= 21; ++i)
        overlong += "S" + std::to_string(i) + " | Subject\n";
    assert(!GroupDefinitionInput::parseSubjects(overlong, parsed));
    assert(parsed.size() == 2); // Failures don't replace a valid earlier result.
    StudentManagement students;
    assert(students.addStudent(makeStudent(7, {90, 80})));
    assert(students.addStudent(makeStudent(8, {70, 70}, "ECE")));
    assert(groups.assign(*students.findStudent(7), "CS25"));
    assert(!groups.assign(*students.findStudent(8), "CS25"));
}

void checkGoal() {
    const Student student = makeStudent(42, {60,70,80,60,60});
    const auto goal = ImprovementGoalPlanner::forPercentage(
        student, 7525, AcademicProcessing::GradeScale::GeuReference);
    assert(goal && goal->requiredTotal == 377 && goal->proposedTotal == 377);
    assert(student.marks()[0] == 60); // Hypothetical, not a record change.
    const auto impossible = ImprovementGoalPlanner::forPercentage(
        student, 9500, AcademicProcessing::GradeScale::GeuReference, {70,80,90,70,70});
    assert(impossible && impossible->status == ImprovementGoalPlanner::Status::Unreachable);
    assert(!ImprovementGoalPlanner::forPercentage(
        student, 10001, AcademicProcessing::GradeScale::GeuReference));
}
} // namespace

int main() {
    checkSearchAndSort();
    checkRanking();
    checkGroupReportsAndSimulation();
    checkGoal();
    checkGroupInput();
    std::cout << "All core checks passed.\n";
}
