// Offline, standard-library-only core regression tests for EduTrack.
// Compile with -std=c++17 -Wall -Wextra -pedantic alongside the core .cpp files.
#include "AcademicGroups.h"
#include "AcademicProcessing.h"
#include "ChangeHistory.h"
#include "RecordAlgorithms.h"
#include "StudentManagement.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
Student make(int roll, const char* name, const char* branch,
             int semester, std::vector<int> marks) {
    return Student(roll, name, branch, semester, std::move(marks));
}

void testSortedSearchAndUndo() {
    StudentManagement records;
    assert(records.addStudent(make(30,"Thirty","CSE",3,{80,90})));
    assert(records.addStudent(make(10,"Ten","CSE",3,{50,60})));
    assert(records.addStudent(make(20,"Twenty","CSE",3,{75,75})));
    assert(!records.addStudent(make(20,"Duplicate","CSE",3,{10})));
    const auto& a=records.allStudents();
    assert(a.size()==3 && a[0].rollNumber()==10 &&
           a[1].rollNumber()==20 && a[2].rollNumber()==30);
    for(int r : {10,20,30}) {
        assert(records.findStudent(r));
        assert(RecordAlgorithms::linearSearch(a,r)==RecordAlgorithms::binarySearch(a,r));
    }
    assert(RecordAlgorithms::binarySearch(a,19)==nullptr);
    assert(RecordAlgorithms::binarySearch(a,31)==nullptr);
    assert(records.updateStudent(20,make(20,"Updated","CSE",3,{91,91})));
    assert(records.findStudent(20)->name()=="Updated");
    assert(!records.updateStudent(20,make(21,"Wrong key","CSE",3,{91})));
    ChangeHistory history;
    assert(history.deleteStudent(records,20));
    assert(records.findStudent(20)==nullptr);
    assert(history.undo(records));
    assert(records.findStudent(20) && records.findStudent(20)->name()=="Updated");
    assert(RecordAlgorithms::binarySearch(records.allStudents(),20)==records.findStudent(20));
    assert(records.deleteStudent(10) && !records.findStudent(10));
    assert(records.allStudents().front().rollNumber()==20);
    assert(records.addStudent(make(10,"Restored","CSE",3,{65})));
    assert(records.allStudents().front().rollNumber()==10);
}

void testRankingTies() {
    std::vector<Student> students{
        make(401,"Four","CSE",3,{75}),
        make(202,"Two","CSE",3,{70,90}),
        make(101,"One","CSE",3,{80}),
        make(303,"Three","CSE",3,{80,80}),
        make(505,"Five","CSE",3,{55})
    };
    const auto ranks=RecordAlgorithms::rankList(students);
    assert(ranks.size()==5);
    assert(ranks[0].rank==1 && ranks[1].rank==1 && ranks[2].rank==1);
    assert(ranks[3].rank==4 && ranks[4].rank==5);
    assert(ranks[0].student->rollNumber()==202); // 160 marks wins display tie break
    assert(RecordAlgorithms::rankOf(students,303)==1);
    assert(RecordAlgorithms::rankOf(students,401)==4);
    assert(!RecordAlgorithms::rankOf(students,999));
    const auto pedagogical=RecordAlgorithms::insertionSortByRollNumber(students);
    assert(std::is_sorted(pedagogical.begin(),pedagogical.end(),
        [](const Student* a,const Student* b){return a->rollNumber()<b->rollNumber();}));
}

void testSubjectsAreBoundToVerifiedGroups() {
    StudentManagement records;
    const Student a=make(101,"Asha","CSE",3,{90,70});
    const Student b=make(102,"Bala","CSE",3,{70,90});
    const Student c=make(201,"Chirag","ECE",3,{20,40});
    const Student d=make(202,"Deep","ECE",3,{40,80});
    const Student unassigned=make(203,"Unassigned","ECE",3,{99,99});
    for(const Student& s : {a,b,c,d,unassigned})assert(records.addStudent(s));
    AcademicGroups groups;
    assert(groups.addGroup({"CSE25","CSE",3,"2025",{{"CS101","Programming"},{"MA101","Mathematics"}}}));
    assert(groups.addGroup({"ECE25","ECE",3,"2025",{{"EC101","Electronics"},{"MA101","Mathematics"}}}));
    for(const auto& pair : std::vector<std::pair<int,const char*>>{
            {101,"CSE25"},{102,"CSE25"},{201,"ECE25"},{202,"ECE25"}})
        assert(groups.assign(*records.findStudent(pair.first),pair.second));
    assert(!groups.assign(*records.findStudent(201),"CSE25"));
    const auto results=AcademicProcessing::groupedSubjectResults(records.allStudents(),groups);
    assert(results.size()==4); // two codes in two groups, no cross-branch mixing
    assert(results[0].groupId=="CSE25" && results[0].subjectCode=="CS101");
    assert(results[0].students==2 && std::abs(results[0].average-80.0)<1e-8);
    assert(results[1].subjectCode=="MA101" && std::abs(results[1].average-80.0)<1e-8);
    assert(results[2].groupId=="ECE25" && results[2].subjectCode=="EC101");
    assert(results[2].students==2 && std::abs(results[2].average-30.0)<1e-8);
    assert(results[3].subjectCode=="MA101" && std::abs(results[3].average-60.0)<1e-8);
    assert(AcademicProcessing::unassignedSubjectRecords(records.allStudents(),groups)==1);
    // A changed identity invalidates the assignment rather than silently remapping marks.
    assert(records.updateStudent(101,make(101,"Not Asha","CSE",3,{90,70})));
    assert(!groups.groupFor(*records.findStudent(101)));
    assert(AcademicProcessing::unassignedSubjectRecords(records.allStudents(),groups)==2);
}

void testSortingScale() {
    StudentManagement records;
    // Comparisons are data-independent, no insertion sort before binary lookup.
    for(int roll=3000;roll>=1;--roll)assert(records.addStudent(
        make(roll,"Student","CSE",3,{roll%101})));
    for(int roll=1;roll<=3000;roll+=3){
        const Student* fast=RecordAlgorithms::binarySearch(records.allStudents(),roll);
        const Student* linear=RecordAlgorithms::linearSearch(records.allStudents(),roll);
        assert(fast && fast==linear);
    }
    const auto ranked=RecordAlgorithms::rankList(records.allStudents());
    assert(ranked.size()==3000);
}

} // namespace

int main(){
    testSortedSearchAndUndo();
    testRankingTies();
    testSubjectsAreBoundToVerifiedGroups();
    testSortingScale();
    std::cout << "PASS: search/undo, competition ties, group-keyed subjects and scale\n";
}
