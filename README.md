# EduTrack

**Phase III — C++ Student Records and Academic Improvement Tool**

EduTrack is a four-member B.Tech PBL project. It stores student marks and uses
basic data structures and algorithms to search, sort, rank, and review them.
Phase III adds named academic groups, subject comparisons, a what-if marks
simulator and a target-mark planner. These are numerical tools, **not AI
predictions** or official university grade calculations.

## What the project does

- **Student records:** add, edit, delete, find, sort, rank and save local marks.
- **DSA:** vectors for records, insertion sort, linear/binary search, a stack
  of previous record states for undo, and a FIFO request queue.
- **Academic groups:** define a branch, semester, cohort and ordered subject
  list, then explicitly assign compatible students. Unassigned students are
  excluded from peer comparisons.
- **Insights:** compare a student with *other* students in the same verified
  group, reporting subject gaps without claiming to know their causes.
- **What-if simulator:** change one subject mark hypothetically and calculate
  a new percentage, illustrative grade and, where enough comparable students
  exist, a group rank. The original marks are never changed.
- **Goal planner:** calculate the minimum total marks for a requested percentage
  or illustrative grade and one possible allocation of improvements. Optional
  subject ceilings flag unreachable targets.
- **Reports:** student and class averages, subject summaries and grade bands.

## Team modules

| Member | Assigned area | Files |
| --- | --- | --- |
| Mohammad Rayyan Farooqui (lead) | Student model, record management, integration | `Student.*`, `StudentManagement.*`, `main.cpp` |
| Shiv Tyagi | Searching, sorting, ranking | `RecordAlgorithms.*` |
| Bhaskar Bahukhandi | Undo and FIFO requests | `ChangeHistory.*`, `RequestQueue.*` |
| Aditya Kukrety | Marks analysis, reports, file storage | `AcademicProcessing.*`, `RecordStorage.*` |

Phase-III components extend these areas; the table describes the original
module split, not a claim that one person wrote every new feature.

## Build and run

C++17 is required. The **console** works on GCC or Clang without third-party
libraries. From the `EduTrack` directory:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
./edutrack
```

**Windows PowerShell (console):**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude (Get-ChildItem src -Filter *.cpp | ForEach-Object FullName) -o EduTrackConsole.exe
.\EduTrackConsole.exe
```

**Windows PowerShell (Phase-III hybrid desktop):**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -municode -mwindows -Iinclude .\gui\Phase3HybridGUI.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) -lcomctl32 -o EduTrackHybrid.exe
.\EduTrackHybrid.exe
```

Only compile **one GUI entry point at a time**. Earlier GUI versions remain
available in `gui/WindowsGUI.cpp` (Phase II) and `gui/Phase3GUI.cpp` (Phase III
read-only preview). The hybrid version combines a familiar records workspace
with the academic analysis pages.

## Using EduTrack

The hybrid desktop has Students, Queue, Reports, Grading, Academic Groups,
Academic Insights, What-if Simulator, Goal Planner, Dashboard and Help. Add or
edit marks through the Students form. In the Queue section, requests are
processed in FIFO order; a request blocked by a group assignment remains
pending. Use Undo for a successful record change when reversing it would not
invalidate an assignment. The console contains the same DSA workflows and is
also used to **create new academic-group definitions**.

Select the same subjects, in the same order, for students in a group. Academic
groups are explicitly assigned, not inferred. Comparative group rank is only
available when there is at least one other valid assigned student.

## Saved files and precautions

- `students.txt` uses the original **`EDUTRACK 1`** format; it remains readable
  by the Phase-II console. Student marks are whole numbers from 0 to 100;
  subjects are equally weighted.
- `academic_groups.txt` is a separate **`EDUTRACK GROUPS 1`** sidecar containing
  academic group definitions and assignments. Group changes do not alter the
  student's saved marks.
- Both files are local to the directory from which the application runs. Save
  and reload using the same working directory.
- `.tmp` and `.bak` files can appear after an interrupted save. **Do not delete
  these blindly.** Inspect and recover data before opening or reloading.
- The hybrid GUI checks file snapshots before writing and blocks further edits
  after an uncertain save. The console also refuses recovery files at startup
  **and on Reload (menu 19)**.
- The two files do **not** form an atomic database transaction, and there is no
  shared lock. **Never run the console and either GUI simultaneously.** Back up
  both files before trying new record/group editing features.
- The FIFO queue and undo history exist only in memory and are lost on exit or
  reload. Hypothetical analysis is always read-only.
- Student identity is currently roll number plus name, not an immutable
  institution-issued identifier. Do not reuse old student identities casually.

## Grading notes

Two **illustrative**, selectable schemes are provided. The original EduTrack
scale is A 90+, B 80+, C 70+, D 60+, F below 60. The other scheme uses the
published GEU *distance-learning* reference table: O above 85, A+ 75+, A 65+,
B+ 55+, B 50+, C 45+, P 40+, F below 40. An exactly 85% score is treated as
A+ because the source leaves that boundary unclear. The reference is **not
verified for our B.Tech batch**, so displayed grades and points must not be
represented as official SGPA or course results.

Reference: Graphic Era (Deemed to be University), distance-learning Programme
Project Report: https://files.geu.ac.in/odl/CIQA%202024/ppr.pdf

## Current review status — Phase III Stage 3G1

The C++17 console builds were checked with GCC and Clang. Local regressions
cover group compatibility, guarded undo and queue processing, simulations,
percentage targets, storage round trips, and reload recovery checks. The latest
console reload fix rejects leftover `.tmp` and `.bak` files and permits a clean
empty-project reload before `students.txt` has been created.

**Not yet cleared for final release:** the current hybrid GUI has not received
an independently verified native Windows compile and full interactive visual
check in this review environment. A previous Phase-III preview ran on Windows,
but that does not validate the latest hybrid editor. Phase II is preserved on
`main`; Phase III changes are on staged review branches.

**Windows test before merging:** build the hybrid GUI; try minimum size,
maximize/restore and navigation; add/edit/delete a student; queue several
requests; process next; undo; assign a student to a compatible group and verify
an incompatible change is refused; save and restart; verify marks and group
names; test what-if and goal results without modifying saved files. Check
keyboard navigation and text repainting. Keep a copy of both saved files
before testing any recovery scenario.

This is a small educational desktop application, not a complete ERP system.


## Stage 3G2A: SQLite foundation (migration preview)

Phase III is **not final**: the application still uses its legacy files by default.
The new `SqliteStorage` component and `migrate_to_sqlite` command provide a
transactional, relational database **foundation**. The next step must switch
**all** console and GUI persistence calls to one database; do not delete any
legacy files yet. The SQL file is not encrypted by default.

To compile the standalone importer on a machine with SQLite headers and library:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude \
    tools/migrate_to_sqlite.cpp database/SqliteStorage.cpp \
    $(ls src/*.cpp | grep -v '/main.cpp') -lsqlite3 -o edutrack-migrate
./edutrack-migrate students.txt academic_groups.txt edutrack.db
```

PowerShell (install the SQLite development headers/library matching your compiler):

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude .\tools\migrate_to_sqlite.cpp .\database\SqliteStorage.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) -lsqlite3 -o edutrack-migrate.exe
.\edutrack-migrate.exe students.txt academic_groups.txt edutrack.db
```

The importer refuses an existing database, invalid legacy files, and old
`.tmp/.bak` recovery files. It preserves both original source files. The
SQLite layer supports atomic **combined snapshot** save/load using prepared
statements, primary/foreign keys and validation. Its full-snapshot save is a
compatibility step; later integration will add targeted SQL CRUD and a backup
workflow. Do not run the old GUI/console while migrating. Keep private data and
SQL database files outside version control. Encryption and user permissions
require a separate deployment decision, not merely use of SQL.
