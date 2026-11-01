#ifndef GUARDED_RECORD_WORKFLOW_H
#define GUARDED_RECORD_WORKFLOW_H

#include "AcademicGroups.h"
#include "ChangeHistory.h"
#include "RequestQueue.h"

#include <optional>

// Portable mutation rules shared with the desktop workflow. The caller stages
// copies of these objects and persists records only after Status::Applied.
// Hypothetical academic calculations never go through this class.
namespace GuardedRecordWorkflow {
enum class Change { Add, Update, Delete, Undo };
enum class Status { Applied, Invalid, GroupConflict, NoHistory, QueueEmpty, RejectedRemoved };

Status apply(Change change, int rollNumber, const std::optional<Student>& replacement,
             const AcademicGroups& groups, StudentManagement& records,
             ChangeHistory& history);

// A failed ordinary request is consumed, just like the original queue.
// A request blocked by academic-group metadata stays at the front.
Status processNext(const AcademicGroups& groups, StudentManagement& records,
                   ChangeHistory& history, RequestQueue& queue);
}
#endif
