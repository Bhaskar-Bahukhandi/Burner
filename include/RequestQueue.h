#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include "ChangeHistory.h"

#include <cstddef>
#include <optional>
#include <queue>
#include <vector>

enum class RequestType { Add, Update, Delete };

struct RequestInfo {
    RequestType type;
    int rollNumber;
};

struct ProcessResult {
    RequestInfo request;
    bool applied;
};

class RequestQueue {
public:
    void enqueueAdd(const Student& student);
    void enqueueUpdate(const Student& student);
    void enqueueDelete(int rollNumber);

    // Failed requests are also removed, and reported as rejected.
    std::optional<ProcessResult> processNext(StudentManagement& records,
                                             ChangeHistory& history);
    std::vector<RequestInfo> pending() const;
    std::size_t size() const;
    void clear();

private:
    struct Request {
        RequestType type;
        int rollNumber;
        std::optional<Student> student;
    };

    std::queue<Request> requests_;
};

#endif
