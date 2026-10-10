# EduTrack

Student performance and academic record management project for third-semester Data Structures with C (TCS-302) and Object Oriented Programming with C++ (TCS-307).

**Current stage: Part 1 — student records and main menu.** This is a working starting point, not the finished Phase-II application. Records are held in memory and are lost when the program exits. File storage is planned for Part 4.

## Build

Use a C++17 compiler. From the project folder:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
```

On Windows, add `.exe` after `edutrack` in the command and run `edutrack.exe`. On Linux/macOS, run `./edutrack`.

## What works now

- Add a student with roll number, name, branch, semester and subject marks.
- Display the current records and find one by roll number.
- Update a student's details or delete a record.
- Reject duplicate roll numbers, missing records and invalid marks or inputs.

Each subject mark is between 0 and 100. One student can have 1 to 20 subjects. Roll numbers cannot be changed during an update.

## Phase-II work plan

| Part | Responsibility | Work |
| --- | --- | --- |
| 1 | Mohammad Rayyan Farooqui — record management and main menu | `Student`, `StudentManagement`, vector storage, add/display/find/update/delete |
| 2 | Shiv Tyagi — searching, sorting and ranking | Linear/binary search, sort by marks/percentage, rank list and tie handling |
| 3 | Bhaskar Bahukhandi — stack and queue | Undo for changes and first-in-first-out request processing |
| 4 | Aditya Kukrety — results, reports and files | Total, percentage, grades, performance bands, subject/class analysis, save/load |
| 5 | Whole team — integration | Connect modules to the menu, handle boundary cases and review the workflow |
| 6 | Whole team — final checks | Compile, run test scenarios, review code and update the Phase-II documentation |

**Module responsibility** follows the project presentation and Phase-II report. Actual contributions and tests should be recorded as they happen.

## Folders

- `include/` — class declarations
- `src/` — C++ source files, including the menu

There are no external libraries or database dependencies. Generated executables and local record files are excluded from version control.
