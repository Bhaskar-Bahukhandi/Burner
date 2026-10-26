#include "GuardedRecordWorkflow.h"

#include <utility>

namespace GuardedRecordWorkflow {
Status apply(Change change, int rollNumber, const std::optional<Student>& replacement,
             const AcademicGroups& groups, StudentManagement& records,
             ChangeHistory& history) {
    StudentManagement candidate = records;
    ChangeHistory stagedHistory = history;
    bool applied = false;
    if (change == Change::Undo) {
        if (stagedHistory.undoCount() == 0) return Status::NoHistory;
        applied = stagedHistory.undo(candidate);
    } else if (change == Change::Add && replacement) {
        applied = stagedHistory.addStudent(candidate, *replacement);
    } else if (change == Change::Update && replacement) {
        applied = stagedHistory.updateStudent(candidate, rollNumber, *replacement);
    } else if (change == Change::Delete) {
        applied = stagedHistory.deleteStudent(candidate, rollNumber);
    }
    if (!applied) return Status::Invalid;
    if (!groups.consistentWith(candidate)) return Status::GroupConflict;
    records = std::move(candidate);
    history = std::move(stagedHistory);
    return Status::Applied;
}

Status processNext(const AcademicGroups& groups, StudentManagement& records,
                   ChangeHistory& history, RequestQueue& queue) {
    if (queue.size() == 0) return Status::QueueEmpty;
    StudentManagement stagedRecords = records;
    ChangeHistory stagedHistory = history;
    RequestQueue stagedQueue = queue;
    const auto outcome = stagedQueue.processNext(stagedRecords, stagedHistory);
    if (!outcome) return Status::QueueEmpty;
    if (outcome->applied && !groups.consistentWith(stagedRecords))
        return Status::GroupConflict; // Do not consume the front request.
    if (!outcome->applied) {
        queue = std::move(stagedQueue);
        return Status::RejectedRemoved;
    }
    records = std::move(stagedRecords);
    history = std::move(stagedHistory);
    queue = std::move(stagedQueue);
    return Status::Applied;
}
}  // namespace GuardedRecordWorkflow
