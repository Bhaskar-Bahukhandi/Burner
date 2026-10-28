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

## Build and run — SQLite desktop

C++17 plus the **SQLite3 development headers and matching library** are now
required for the main console and the hybrid GUI. On Windows, the compiler,
SQLite headers and libraries must all come from the **same MinGW environment**.
For a fresh MSYS2 installation, use the **UCRT64** terminal (not the deprecated
MINGW64 environment) and install matching packages:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3
```

In PowerShell, verify that `g++` resolves to the matching
`C:\msys64\ucrt64\bin\g++.exe` (for example with `where.exe g++`).
If you have a different MinGW toolchain, install SQLite headers and libraries
built for that toolchain instead; do not mix environments. The MSYS2 UCRT64
shell can also run the commands with forward-slash paths. Official guidance:
https://www.msys2.org/ and
https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-sqlite3.
A standalone SQLite command-line executable alone does not provide the link library.

Linux (run from the project root):

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
    src/*.cpp database/SqliteStorage.cpp -lsqlite3 -o edutrack
./edutrack
```

Windows PowerShell — **SQLite console**:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude (Get-ChildItem src -Filter *.cpp | ForEach-Object FullName) .\database\SqliteStorage.cpp -lsqlite3 -o EduTrackConsole.exe
.\EduTrackConsole.exe
```

Windows PowerShell — **SQLite hybrid GUI**:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -municode -mwindows -Iinclude .\gui\Phase3HybridGUI.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) .\database\SqliteStorage.cpp -lsqlite3 -lcomctl32 -o EduTrackHybrid.exe
.\EduTrackHybrid.exe
```

Do **not** link the different GUI entry points together. The previous Windows
GUI files remain only as development checkpoints; they still use legacy text
files and must not be used for editing the imported SQL dataset.

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

## SQLite records, migration and safeguards

**The live backend for the Phase-III console and hybrid GUI is `edutrack.db`.**
Students, ordered marks, academic groups, named subjects and assignments are
related by checked keys in one SQLite database. A new database is created only
when no SQLite database **and** no legacy student/group text files are present.

**To upgrade an existing Phase-II/III dataset:**

1. Close all EduTrack console and GUI windows. Back up both `students.txt` and
   `academic_groups.txt` (if present). Inspect any leftover `.tmp/.bak` first.
2. Build the standalone migration tool using the commands below.
3. Run the importer in the directory containing the two legacy files. The
   SQLite target **must not already exist**. The importer checks every loaded
   mark, subject and assignment after writing the database.
4. Open the newly built **SQL console or SQL hybrid GUI** and verify the record
   counts, assigned groups and marks. Keep the old text files as recovery copies.

Linux:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
    tools/migrate_to_sqlite.cpp database/SqliteStorage.cpp \
    src/Student.cpp src/StudentManagement.cpp src/AcademicGroups.cpp \
    src/RecordStorage.cpp src/AcademicGroupStorage.cpp -lsqlite3 -o edutrack-migrate
./edutrack-migrate students.txt academic_groups.txt edutrack.db
```

Windows PowerShell:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude .\tools\migrate_to_sqlite.cpp .\database\SqliteStorage.cpp .\src\Student.cpp .\src\StudentManagement.cpp .\src\AcademicGroups.cpp .\src\RecordStorage.cpp .\src\AcademicGroupStorage.cpp -lsqlite3 -o edutrack-migrate.exe
.\edutrack-migrate.exe students.txt academic_groups.txt edutrack.db
```

**Consistency:** SQLite commits record and group changes in one transaction.
The one-time `SqliteStorage::save()` initializer **refuses existing database
files and leftover WAL/SHM/journal files**; to edit a previously created
project, use `SqliteStorage::commit()` instead. The importer must target a new
filename. The `SqliteStorage::commit()` function checks the caller's entire expected
snapshot *inside* a write transaction and rejects newer external changes;
only affected record/mark/group/assignment rows are updated. A failed
transaction leaves the previous committed state intact. Reload after a
conflict. Using one database eliminates the old separate-file atomicity gap.
Undo history and queued requests remain in memory, not in SQLite.

**Privacy:** SQLite is local and supports database constraints and transactions
but **does not encrypt records, authenticate users, or provide server-side
access controls by itself**. Protect the file with operating-system permissions,
keep it out of Git, and avoid storing real private academic data in demos. For
multi-user internet access, a server backend and authentication would be
necessary. The Stage 3G2C maintenance tool now supplies verified SQLite online backups
and safe restore-to-new-file. It does **not** encrypt the database. Use Windows
account/file permissions or full-disk encryption for local protection, and
store backup copies only in access-controlled locations. For stronger protection
of a shared machine, plan a separately tested SQLCipher integration. Do not
copy only a SQLite WAL sidecar or use backups as an authentication system.

## SQLite backup, verification and recovery — Stage 3G2C

The standalone `edutrack-db` tool is the **safest way to back up the current SQLite
records**. It uses the SQLite online backup API rather than copying the main
`.db` file while a WAL file may contain committed changes. Close both app
windows before running a **restore**. The tool never silently overwrites a
previous database or backup, or publishes a copy beside **stale destination
WAL/SHM/journal files**; every new copy receives a complete integrity,
foreign-key and EduTrack record validation before becoming available.

Linux build (in the EduTrack directory):

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
    tools/database_maintenance.cpp database/DatabaseMaintenance.cpp \
    database/SqliteStorage.cpp src/Student.cpp src/StudentManagement.cpp \
    src/AcademicGroups.cpp src/RecordStorage.cpp src/AcademicGroupStorage.cpp \
    -lsqlite3 -o edutrack-db
```

Windows PowerShell (with the matching SQLite3 development library installed):

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude .\tools\database_maintenance.cpp .\database\DatabaseMaintenance.cpp .\database\SqliteStorage.cpp .\src\Student.cpp .\src\StudentManagement.cpp .\src\AcademicGroups.cpp .\src\RecordStorage.cpp .\src\AcademicGroupStorage.cpp -lsqlite3 -o edutrack-db.exe
```

Commands (replace `./edutrack-db` with `.\edutrack-db.exe` on Windows):

```sh
./edutrack-db verify edutrack.db
./edutrack-db backup edutrack.db backup-2026-10-28.db
./edutrack-db verify backup-2026-10-28.db
./edutrack-db restore backup-2026-10-28.db edutrack-restored.db
./edutrack-db verify edutrack-restored.db
```

**Restore activation is intentionally manual.** `restore` creates
`edutrack-restored.db` and leaves a working `edutrack.db` untouched. After
closing all EduTrack programs, preserve the existing `edutrack.db` **and any
matching `-wal`/`-shm`/`-journal` files as a set**, confirm the restored record
counts, and arrange an offline filename switch. Move the old database and all
its matching sidecars away from the active filename before renaming the
verified restored file to `edutrack.db`; never leave stale `edutrack.db-wal`
or `edutrack.db-shm` files next to the new database. Do not replace a live
WAL-mode database while the application is running. Never delete the original
or source backup until you have reopened and verified the restored data.

This tool is not an encrypted vault or cloud service; on Unix it creates new
backup files with owner-only read/write bits, while Windows confidentiality
continues to depend on NTFS account permissions/drive encryption. Keep all
personal `.db`, `.sqlite`, backup and journal files out of Git. Password
protection, SQLCipher encryption, authenticated multi-user access and
an automatic GUI restore wizard remain **unimplemented**.

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

## Phase-III release status — Stage 3G3A audit checkpoint

The SQL console and the new Phase-III hybrid GUI now use `edutrack.db` as their
primary data source. The standalone importer remains non-destructive: it never
changes the original text files and rejects a pre-existing SQLite target.
The SQL commit path uses bound parameters, foreign keys, integrity checks,
transactional rollback, and compares snapshots under the SQLite write lock.

**Not yet final:** the current hybrid source still needs a native Windows/MinGW
compile and interactive visual/runtime validation. Linux GCC/Clang builds and
portable SQLite regression tests do not establish that the Windows GUI builds
or renders correctly. SQLite backup and verify/restore-to-new-file are available in the maintenance
tool; encryption and native Windows backup/restore testing remain separate release
gates. The Stage 3G3A audit hardened two risks: **new database initialization
cannot overwrite a committed database**, including when two initializers race;
and **backup/restore refuses stale sidecars at the destination**. Both are
covered by GCC/Clang regression tests, but these are not Windows UI tests.
The older console and GUI versions on Phase-II `main` are
preserved for rollback, not for simultaneously editing SQL data.

**Required Windows test:** compile using the command above; open the imported
SQLite database; verify class/subject/group counts; add, update and delete a
student; process a queued request and undo; assign/unassign academic groups;
restart and verify persistence; confirm simulated marks are not saved; test a
second running instance attempting a stale update; resize/maximize/minimize;
verify keyboard and text rendering. Make a recovery copy before testing.

This is an educational local desktop project, not a complete ERP system.
