# EduTrack

EduTrack is a C++ console program for keeping student records and checking academic performance. It was planned for the third-semester PBL work in Data Structures with C (TCS-302) and OOP with C++ (TCS-307).

## Project features

- Add, view, search, update and delete student records.
- Store subject marks and calculate total marks, percentage and grades.
- Search by roll number using linear or binary search.
- Sort students and display the rank list and top performer.
- Show performance categories, subject-wise results and class-average comparisons.
- Use a stack to undo changes and a queue to process record requests in order.
- Save records in `students.txt` and load them when the program starts again.

## Team modules

| Team member | Responsibility | Files |
| --- | --- | --- |
| Mohammad Rayyan Farooqui (Team Lead) | Student class, vector-based record management, main menu and integration | `Student.*`, `StudentManagement.*`, `main.cpp` |
| Shiv Tyagi | Searching, sorting and rank calculation | `RecordAlgorithms.*` |
| Bhaskar Bahukhandi | Stack-based undo and request queue | `ChangeHistory.*`, `RequestQueue.*` |
| Aditya Kukrety | Grades, academic reports, saving and loading records | `AcademicProcessing.*`, `RecordStorage.*` |

The code is separated into `include/` (headers) and `src/` (implementations). Each member's ZIP contains their assigned module. **Extract all four ZIP files into the same project folder** to get the complete application. The README and `.gitignore` are included in the team lead's ZIP only.

## Compile and run

A C++17 compiler is required. No external libraries or database are needed.

Linux/macOS:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
./edutrack
```

Windows with MinGW-w64 and PowerShell:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude (Get-ChildItem src -Filter *.cpp | ForEach-Object FullName) -o edutrack.exe
.\edutrack.exe
```

Run the program from its project folder so it can find `students.txt`. Use the numbered menu to select an operation.

## How results are calculated

Each subject is marked out of 100 and has equal weight. Percentage is the total of subject marks divided by the number of subjects. Grades are A (90+), B (80+), C (70+), D (60+) and F (below 60). Performance categories are Excellent (85+), Good (70+), Average (50+) and Needs Improvement (below 50). **These are project rules, not confirmed university grading rules.**

Ranks are ordered by higher percentage, then higher total marks, then smaller roll number. Subject reports use `Subject 1`, `Subject 2`, etc., rather than subject names, so marks should be entered in the same subject order for each student.

## Record files

Student records are stored in `students.txt` in the current working folder. The program loads saved records at startup and saves changes when exiting normally; option 18 also saves them immediately.

During a save, the program may create `students.txt.tmp` and `students.txt.bak` to protect the previous copy. If either file remains after an interrupted save, inspect the files and restore the correct records before restarting. Do not delete a backup without checking it first.

Undo history and pending requests are only kept while the program is running; they are not saved to disk. The program asks for confirmation before discarding queued requests during normal exit.