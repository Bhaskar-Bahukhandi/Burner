#include "ChangeHistory.h"
#include "GuardedRecordWorkflow.h"
#include "RequestQueue.h"

#include <cassert>
#include <iostream>

int main() {
    StudentManagement records;
    ChangeHistory history;
    const Student first(11, "Asha", "CSE", 3, {75, 80});
    const Student revised(11, "Asha", "CSE", 3, {95, 80});
    assert(history.addStudent(records, first));
    assert(!history.addStudent(records, first));
    assert(history.updateStudent(records, 11, revised));
    assert(!history.updateStudent(records, 11, Student(12, "Wrong", "CSE", 3, {1})));
    assert(history.deleteStudent(records, 11));
    assert(!records.findStudent(11));
    assert(history.undoCount() == 3);
    assert(history.undo(records));
    assert(records.findStudent(11)->marks()[0] == 95);
    assert(history.undo(records));
    assert(records.findStudent(11)->marks()[0] == 75);
    assert(history.undo(records));
    assert(!records.findStudent(11));
    assert(!history.undo(records));

    // Thousands of changes should store reverse actions, not thousands of
    // copies of all student records.
    assert(history.addStudent(records, first));
    for (int i = 0; i < 4000; ++i) {
        const int mark = i % 101;
        assert(history.updateStudent(records, 11, Student(11, "Asha", "CSE", 3, {mark, 80})));
    }
    assert(history.undoCount() == 4001);
    for (int i = 0; i < 4000; ++i) assert(history.undo(records));
    assert(records.findStudent(11)->marks()[0] == 75);
    history.clear();
    assert(history.undoCount() == 0);

    AcademicGroups groups;
    const AcademicGroup cseGroup{"CSE25", "CSE", 3, "2025",
                                 {{"DS", "Data Structures"}, {"MA", "Mathematics"}}};
    assert(groups.addGroup(cseGroup));
    assert(groups.assign(*records.findStudent(11), "CSE25"));
    ChangeHistory guardedHistory;
    assert(GuardedRecordWorkflow::apply(GuardedRecordWorkflow::Change::Delete, 11,
        std::nullopt, groups, records, guardedHistory) ==
        GuardedRecordWorkflow::Status::GroupConflict);
    assert(records.findStudent(11) && guardedHistory.undoCount() == 0);
    assert(GuardedRecordWorkflow::apply(GuardedRecordWorkflow::Change::Update, 11,
        Student(11, "Asha", "CSE", 3, {65, 80}), groups, records, guardedHistory) ==
        GuardedRecordWorkflow::Status::Applied);
    assert(GuardedRecordWorkflow::apply(GuardedRecordWorkflow::Change::Undo, 0,
        std::nullopt, groups, records, guardedHistory) ==
        GuardedRecordWorkflow::Status::Applied);
    assert(records.findStudent(11)->marks()[0] == 75);

    RequestQueue queue;
    queue.enqueueDelete(11); // assigned student: must remain pending
    assert(GuardedRecordWorkflow::processNext(groups, records, guardedHistory, queue) ==
        GuardedRecordWorkflow::Status::GroupConflict);
    assert(queue.size() == 1);
    assert(groups.unassign(11));
    assert(GuardedRecordWorkflow::processNext(groups, records, guardedHistory, queue) ==
        GuardedRecordWorkflow::Status::Applied);
    assert(queue.size() == 0 && !records.findStudent(11));
    assert(GuardedRecordWorkflow::apply(GuardedRecordWorkflow::Change::Undo, 0,
        std::nullopt, groups, records, guardedHistory) ==
        GuardedRecordWorkflow::Status::Applied);
    assert(records.findStudent(11)->marks()[0] == 75);
    queue.enqueueDelete(123456); // invalid request gets consumed
    assert(GuardedRecordWorkflow::processNext(groups, records, guardedHistory, queue) ==
        GuardedRecordWorkflow::Status::RejectedRemoved);
    assert(queue.size() == 0);
    std::cout << "Undo, queue and group safety checks passed.\n";
}
