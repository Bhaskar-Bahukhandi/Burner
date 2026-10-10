# EduTrack

EduTrack is a console-based student record and academic performance program developed for the third-semester PBL subjects TCS-302 (Data Structures with C) and TCS-307 (OOP with C++).

**Phase-II, Part 5:** the four functional modules are integrated. The integration review covers console workflows, reports, record persistence and safe recovery checks. Final handover, Windows-specific verification and team review are still pending.

## Build

Use a compiler supporting C++17 (recent GCC, Clang or MinGW-w64). Run from the project folder:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
```

Run `./edutrack` on Linux/macOS. On Windows, build with `-o edutrack.exe` and run `edutrack.exe`. The working folder is where `students.txt` will be saved. No external packages or database are needed.

## Menu

| Option | Work |
| --- | --- |
| 1–5 | Add, display, linear-search, update and delete student records |
| 6–10 | Binary search, sort by percentage/total, rank list and rank lookup |
| 11–15 | Queue requests, process them in order and undo completed changes |
| 16 | Student marks, percentage, grade, category and class-average comparison |
| 17 | Class average, topper, performance bands and subject-wise report |
| 18 | Save records now |
| 19 | Reload saved records (with confirmation if the session has changes) |
| 0 | Save changes and exit |

A student has a unique positive roll number (up to 999999999), name, branch, semester (1–12), and 1–20 marks (each from 0 to 100). Roll numbers stay fixed during updates. Searching and sorting do not change the stored order.

## Percentage, grades and performance

Every subject currently has equal weight and is marked out of 100. Therefore **percentage = total subject marks / number of subjects**. These project grade thresholds are not claimed to be the university's official grading scale:

| Percentage | Grade |
| --- | --- |
| 90–100 | A |
| 80–below 90 | B |
| 70–below 80 | C |
| 60–below 70 | D |
| Below 60 | F |

The four performance categories are **Excellent** (85 and above), **Good** (70–below 85), **Average** (50–below 70), and **Needs Improvement** (below 50). These cutoffs are project rules and can be changed if the mentor specifies different ones.

Class average is the average of each student's percentage, so students with different numbers of subjects are weighted equally. Subject-wise averages use only the students who have a mark at that subject position. Since the original student model does not include subject names, reports label them *Subject 1*, *Subject 2*, etc. Such comparisons assume subject positions mean the same subject for every student; keep entry order consistent when entering marks. The class report warns when the numbers of subjects differ between students.

The topper and ranks use descending percentage, then descending total marks, then ascending roll number. Each student gets a consecutive rank even if percentages are equal.

## Records and recovery

Records are stored in the human-readable `students.txt` file in the working folder. The program loads it at startup, and saves changed records when exiting normally. Option 18 saves immediately. If startup finds an invalid record file, it **stops with an error instead of overwriting it**.

Saving first writes `students.txt.tmp`. If an older `students.txt` exists, it is renamed to `students.txt.bak` while the new file is put in place; the backup is then removed. If the write or rename fails, the program reports it. If `.tmp` or `.bak` remains after an interrupted save, the program refuses to start: inspect these files and restore the correct copy manually before trying again. Keep a separate backup when handling important records. This is a small single-user file-based project, not a concurrent database.

Loading validates the header, record count, marks, unique roll numbers and file contents before replacing the active records. Reloading clears undo history and pending requests **only after a successful load**. Undo history and queued requests are **never stored on disk**; any unprocessed requests are lost on exit. The program warns about pending requests before leaving.

## Stack, queue and searching

`ChangeHistory` keeps a stack of complete record snapshots, allowing the last successful add/update/delete to be undone. Failed operations do not create undo entries. `RequestQueue` runs queued add/update/delete requests in FIFO order; rejected requests are reported and removed without changing records. Both use the same record checks.

The search module demonstrates linear search and binary search. Binary search first prepares a roll-number-sorted view. Insertion sort is used for sorted views: its worst-case cost is O(n²). The binary-search view is re-sorted for each lookup, so a single lookup is not faster overall than linear search. Full-copy undo snapshots are also intended for small classroom datasets, not large-scale deployment.

## File ownership by module

The responsibilities below follow the team's Phase-I/Phase-II documents. The team should review the changes and confirm actual contributions before final submission.

| Member | Module | Files |
| --- | --- | --- |
| Mohammad Rayyan Farooqui | Student records, vector, CRUD and main menu | `Student.*`, `StudentManagement.*`, `main.cpp` |
| Shiv Tyagi | Search, sorting, ranking and tie handling | `RecordAlgorithms.*` |
| Bhaskar Bahukhandi | Stack, undo and FIFO request queue | `ChangeHistory.*`, `RequestQueue.*` |
| Aditya Kukrety | Grades, subject/class reports and saved records | `AcademicProcessing.*`, `RecordStorage.*` |

All header files are in `include/`; C++ implementations are in `src/`. Integration regression checks have been performed on Linux with GCC and Clang. Windows-specific validation, final acceptance and report updates belong to the whole team. Temporary tests, build products and student data are not included in the repository.
