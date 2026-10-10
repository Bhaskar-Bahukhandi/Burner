#include "RequestQueue.h"

#include <utility>

void RequestQueue::enqueueAdd(const Student& student) {
    requests_.push({RequestType::Add, student.rollNumber(), student});
}

void RequestQueue::enqueueUpdate(const Student& student) {
    requests_.push({RequestType::Update, student.rollNumber(), student});
}

void RequestQueue::enqueueDelete(int rollNumber) {
    requests_.push({RequestType::Delete, rollNumber, std::nullopt});
}

std::optional<ProcessResult> RequestQueue::processNext(StudentManagement& records,
                                                        ChangeHistory& history) {
    if (requests_.empty()) return std::nullopt;

    const Request& request = requests_.front();
    bool applied = false;
    switch (request.type) {
        case RequestType::Add:
            applied = history.addStudent(records, *request.student);
            break;
        case RequestType::Update:
            applied = history.updateStudent(records, request.rollNumber, *request.student);
            break;
        case RequestType::Delete:
            applied = history.deleteStudent(records, request.rollNumber);
            break;
    }

    ProcessResult result{{request.type, request.rollNumber}, applied};
    requests_.pop();
    return result;
}

std::vector<RequestInfo> RequestQueue::pending() const {
    std::queue<Request> copy = requests_;
    std::vector<RequestInfo> result;
    result.reserve(copy.size());
    while (!copy.empty()) {
        result.push_back({copy.front().type, copy.front().rollNumber});
        copy.pop();
    }
    return result;
}

std::size_t RequestQueue::size() const {
    return requests_.size();
}

void RequestQueue::clear() {
    std::queue<Request> empty;
    requests_.swap(empty);
}
