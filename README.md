# EduTrack

EduTrack is a C++17 desktop project for managing student marks and reviewing academic progress. It combines student-record operations with searching, sorting, a stack for undo, a FIFO request queue, and an SQLite database. Its academic features work with explicitly defined groups and subjects; they do not replace official university results.

**Intended user:** a faculty member or academic coordinator working on one computer. The program does not currently have student logins or separate user roles.

## Features

- Add, edit, delete, search, sort and view student records.
- Queue record changes in FIFO order and undo eligible edits during the current session.
- Define an academic group with a branch, semester, cohort and ordered subject codes; assign matching students.
- Produce group-specific subject reports, a simple bar chart and descriptive peer comparisons. The student table can scroll horizontally when space is limited.
- Calculate hypothetical rank changes, target percentages and example mark allocations without changing saved marks. Optional subject ceilings use the same labelled marks editor.
- Store records, marks, subject definitions and assignments in SQLite with transactions and foreign-key checks.
- Import legacy text records into a new database; verify, back up and restore databases using separate utilities.

Grades and suggested improvements are **illustrative**, not an official B.Tech grading or prediction system. Comparisons of subjects only make sense within a verified group.

## Project layout

```text
EduTrack/
  gui/                 One Windows GUI and its three form headers
  include/             Shared C++ headers
  src/                 Student records, DSA, calculations and console
  database/            SQLite storage and backup implementation
  tools/               Legacy importer and database maintenance CLI
  tests/               Four portable regression programs + Windows GUI smoke test
  docs/                PBL presentation and Windows testing guides
  build_windows.ps1    PowerShell build helper
  run_windows.ps1      Run with the correct UCRT64 runtime DLLs
  README.md
  .gitignore
```

`gui/WindowsGUI.cpp` is the desktop entry point. `src/main.cpp` is the optional console entry point. Build them separately. Both use `edutrack.db`, not the old text files. The legacy text readers are kept only for importing existing records.

## Build on Windows

Install [MSYS2](https://www.msys2.org/), open its **UCRT64** terminal, and install the matching C++ compiler and SQLite development package:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3
```

Reopen UCRT64 if the update asks you to. From **PowerShell**, in the extracted project folder:

```powershell
.\build_windows.ps1 -Target GUI
.\run_windows.ps1 -Target GUI
```

The run helper starts programs from the EduTrack folder with the matching UCRT64 DLL directory. This avoids a missing-DLL error (0xC0000135) and keeps `edutrack.db` in one predictable location. It restores the previous PowerShell working directory and PATH afterwards. To build the GUI, console, importer, maintenance utility and tests together:

```powershell
.\build_windows.ps1 -Target All
```

The build script expects UCRT64 under `C:\msys64\ucrt64`. If yours is elsewhere, pass `-Ucrt64 'D:\msys64\ucrt64'` with the correct path. If scripts are blocked, `Set-ExecutionPolicy -Scope Process Bypass` changes policy for the current PowerShell process only.

If the compiler reports `sqlite3.h: No such file or directory`, install the UCRT64 SQLite development package rather than downloading unrelated DLL files. The build script checks for the header and import library before compiling. The same UCRT64 runtime must be available when the application launches.

## Build the console on Linux

Install a C++17 compiler and the SQLite development headers/library, then run from the project folder:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp database/SqliteStorage.cpp -lsqlite3 -o edutrack
./edutrack
```

The GUI is Windows-specific and is not built by this command.

## Run the tests

On Windows:

```powershell
.\build_windows.ps1 -Target Tests
.\run_windows.ps1 -Target Tests
```

The portable tests cover searching and tie ranking, academic-group calculations, undo/FIFO behavior, database changes and migration, and verified backup/restore. The fifth Windows-only smoke test opens and cancels each editor, checking that its actual child controls were created (including subject limits). Run the tests with **fictional test records**. Even a successful smoke test does not substitute for manual visual and functional testing on Windows.

## First run and old records

The application creates a new `edutrack.db` only when neither an existing database nor legacy records are present. To import old files, close EduTrack, make copies of `students.txt` and `academic_groups.txt`, then build the importer with:

```powershell
.\build_windows.ps1 -Target Importer
.\run_windows.ps1 -Target Importer -Arguments @('students.txt', 'academic_groups.txt', 'edutrack.db')
```

The importer verifies the data in a separate `.importing` database before publishing to a **new** filename. It refuses an existing destination and keeps both source files unchanged. If migration fails, inspect the error and any leftover `.importing` file before retrying.

## Backup and recovery

```powershell
.\build_windows.ps1 -Target Maintenance
.\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'edutrack.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('backup', 'edutrack.db', 'backup.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('restore', 'backup.db', 'restored.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'restored.db')
```

Backup uses SQLite's online backup API, including committed WAL changes. Restore creates a **new database file**, never overwrites the active one. Close all EduTrack processes and preserve the old database and its SQLite sidecar files before manually activating a restored copy.

## Data rules and limitations

- Each student has a unique positive roll number, a branch and semester, and 1–20 integer marks in the range 0–100.
- An academic group specifies the branch, semester and **ordered** subject definitions. Assigned students must match this definition.
- Searching uses roll-ordered records; ranking uses competition ranks (for example 1, 1, 3). Group reports do not mix unrelated departments' subject positions.
- Edits use SQLite transactions and stale-record checks. Queue and undo history exist only until reload/exit.
- The desktop currently stages in-memory copies for some safety checks. This program is designed for a local academic workload, not unlimited simultaneous users.
- SQLite files are **not encrypted by default**. There is no password/login system or multi-user server. Use OS account protection and disk encryption for private records.
- Do not run multiple editing instances simultaneously, and do not use real students' private information in a public demo.

For the project explanation, team module allocation, algorithms and a rehearsal walkthrough, see [Project and demo guide](docs/PROJECT_AND_DEMO.md). Before submission, complete the [Windows verification checklist](docs/WINDOWS_CHECKLIST.md).

## Windows GUI acceptance test

Build the GUI and the Windows dialog test with `-Target All`, then run `.\run_windows.ps1 -Target Tests`. Confirm the student, academic-group, subject-marks, and goal-limits forms display their labels, inputs, Save/Apply and Cancel buttons. Verify that Cancel does not save anything.

In a **new empty database**: use the Dashboard shortcuts to create a group and add a student; enter subject marks; assign the student to the matching group. In Students, verify the table headings and horizontal scrollbar at narrow window widths. Test Edit, Delete confirmation, Undo, FIFO, comparable Reports, Simulator, Goal Planner, and persistence after restart. The actions requiring a student should initially be disabled. Follow `docs/WINDOWS_CHECKLIST.md` for deeper tests.

The Win32 interface must be compiled and exercised on an actual Windows machine before calling these visual and interactive changes verified.
