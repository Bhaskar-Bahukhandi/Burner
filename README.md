# EduTrack

EduTrack is a console-based student academic record management project for third-semester Data Structures with C (TCS-302) and Object Oriented Programming with C++ (TCS-307).

**Current stage: Phase-II Part 3.** Student records, searching, sorting, ranking, undo and queued requests are implemented. Records are still kept in memory and are lost when the program exits. File save/load and academic reports are planned for Part 4.

## Build and run

Install a C++17 compiler and run from the project folder:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
```

On Windows, use `-o edutrack.exe` and run `edutrack.exe`. On Linux/macOS, run `./edutrack`.

## Menu

| Option | Action |
| --- | --- |
| 1–5 | Add, display, linear-search, update or delete students |
| 6 | Binary search by roll number |
| 7–8 | View records sorted by percentage or total marks |
| 9–10 | View rank list or find an individual's rank |
| 11 | Queue an add, update or delete request |
| 12 | Process the oldest waiting request |
| 13 | Show queued requests in processing order |
| 14 | Undo the last successful record change |
| 15 | Process all waiting requests |
| 0 | Exit |

Each record has a positive roll number, non-empty name and branch, a semester from 1 to 12, and 1–20 subject marks between 0 and 100. Roll numbers are unique and cannot be changed by an update.

## Stack and queue

`ChangeHistory` uses `std::stack<StudentManagement>` to hold the complete record state before a **successful** add, update or delete. Undo restores the last snapshot, including the order of records. Failed operations do not add an undo step, and an empty undo stack is handled safely. This approach is straightforward for a small course project, but each undo entry copies the current records, so memory use grows with the number and size of changes.

`RequestQueue` uses `std::queue` to process requests in first-in, first-out order. You can queue an add, update or delete without running it immediately. Processing uses the same validation and undo logic as the normal menu. If a queued request is no longer valid when its turn arrives (for example, a duplicate roll number), the program **reports the rejection and removes that request without changing the records**. Queued requests are not undone; only successful record changes are undone. Pending requests remain in the queue when an earlier operation is undone.

**Example:** queue Add(101), Add(102), Delete(101). Processing all three leaves only roll 102. Undoing once restores roll 101.

The stack and queue are temporary, like the records. They do not survive restarting the program.

## Searching, sorting and ranks

Linear search scans records in insertion order. Binary search creates a roll-number-sorted view and searches it. Sorting does not rearrange the original records.

The rank list is ordered by descending percentage, then descending total marks, then ascending roll number. Ranks are consecutive positions; equal percentages do not automatically share a rank. Each subject counts equally, and percentage is the mean of the subject marks (each out of 100). Insertion sort is used for the sorted views. It is O(n²); preparing a new sorted view for each binary search also takes O(n²), so a single binary search is not faster overall than a linear search in this design.

## Responsibilities and remaining work

| Part | Assigned team member | Module |
| --- | --- | --- |
| 1 | Mohammad Rayyan Farooqui | Student class, vector storage, record management, menu |
| 2 | Shiv Tyagi | Searching, sorting, ranking and tie handling |
| 3 | Bhaskar Bahukhandi | Stack-based undo and FIFO request queue |
| 4 | Aditya Kukrety | Grades, academic analysis and file handling (pending) |
| 5–6 | Whole team | Integration, validation and final documentation (pending) |

These are module assignments from the project report. Team members should review and understand their modules, and document their actual contributions.

## Files

```text
include/
    Student.h
    StudentManagement.h
    RecordAlgorithms.h
    ChangeHistory.h
    RequestQueue.h
src/
    Student.cpp
    StudentManagement.cpp
    RecordAlgorithms.cpp
    ChangeHistory.cpp
    RequestQueue.cpp
    main.cpp
```

No third-party libraries or database are required. Build products and local data files are excluded from Git through `.gitignore`.
