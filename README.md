# EduTrack

Student performance and academic record management project for third-semester Data Structures with C (TCS-302) and Object Oriented Programming with C++ (TCS-307).

**Current stage: Part 2 — searching, sorting and ranking.** Records are still kept in memory; closing the application discards them. File storage is planned for Part 4.

## Build and run

Use a C++17 compiler. Run this command from the project folder:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
```

On Windows, use `-o edutrack.exe` and run `edutrack.exe`. On Linux or macOS, run `./edutrack`.

## Current menu

| Option | Action |
| --- | --- |
| 1–5 | Add, display, linear-search, update or delete students |
| 6 | Binary search for a roll number |
| 7–8 | View records sorted by percentage or total marks |
| 9 | Display a ranked list |
| 10 | Find a student's rank using a roll number |
| 0 | Exit |

The program checks invalid marks, duplicate roll numbers, missing records and other basic input mistakes. Each subject mark is out of 100; students may have 1 to 20 subjects. Roll numbers cannot be changed through the update option.

## Sorting and ranking rules

- The percentage is the average of the subject marks, since each subject is out of 100. Every subject has equal weight.
- **Rank order:** higher percentage first. If percentages match exactly, higher total marks comes first. If those also match, the smaller roll number comes first.
- Ranks are consecutive positions (1, 2, 3, ...), with no shared ranks. This applies the tie-break rule described in the Phase-II report.
- **Total-mark sort:** higher total first; equal totals are ordered by percentage and then roll number.
- Linear search checks the stored vector directly. Binary search first creates a roll-number-sorted view, then searches that view. The original vector stays unchanged.
- These views contain pointers to stored records and are only used immediately. They are not retained after an add, update or delete.

The sorting uses insertion sort, which is easy to inspect for this course project but has O(n²) worst-case time. Binary search itself is O(log n), but preparing its sorted view takes O(n²) each time, so it is **not** presented as faster overall than linear search for a single lookup. If the application grows, sorting once and safely reusing an index would be more efficient.

## Project parts and responsibilities

| Part | Responsibility | Work |
| --- | --- | --- |
| 1 | Mohammad Rayyan Farooqui — record management and main menu | `Student`, `StudentManagement`, vector storage and CRUD |
| 2 | Shiv Tyagi — searching, sorting and ranking | `RecordAlgorithms`, linear/binary search, insertion sort and rank lookup |
| 3 | Bhaskar Bahukhandi — stack and queue | Undo history and queued requests |
| 4 | Aditya Kukrety — results, reports and files | Grades, analysis, subject reports and file save/load |
| 5 | Whole team — integration | Connect modules and handle boundary cases |
| 6 | Whole team — final checks | Test complete workflows, review code, update documentation |

Module assignments follow the project presentation. Members should review the changes and record their actual work before submission.

## Project files

- `include/Student.h`, `src/Student.cpp`: student data model.
- `include/StudentManagement.h`, `src/StudentManagement.cpp`: record operations.
- `include/RecordAlgorithms.h`, `src/RecordAlgorithms.cpp`: searching, sorting and ranking.
- `src/main.cpp`: console interface.

No external packages or database are required. Local executables and saved student data are excluded by `.gitignore`.
