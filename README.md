# EduTrack

EduTrack keeps student records, marks and academic reports in a C++ application. It has a console menu and an optional Windows desktop interface. The project uses classes, vectors, searching, sorting, stacks, queues and file handling for the TCS-302 / TCS-307 Phase-II PBL.

## Features

- Add, find, update and delete student details and marks.
- Search by roll number, sort by marks or percentage, and display ranks.
- Undo successful changes using a stack and process queued requests in order.
- Display student and class reports, including subject-wise averages.
- Save records in `students.txt` and reload them after restarting.
- Change between two **illustrative** grading schemes without changing saved marks.

## Team modules

| Member | Work | Code |
| --- | --- | --- |
| Mohammad Rayyan Farooqui (lead) | Student model, record management, console menu | `Student.*`, `StudentManagement.*`, `main.cpp` |
| Shiv Tyagi | Searching, sorting, ranking | `RecordAlgorithms.*` |
| Bhaskar Bahukhandi | Stack, undo and request queue | `ChangeHistory.*`, `RequestQueue.*` |
| Aditya Kukrety | Academic reports and file handling | `AcademicProcessing.*`, `RecordStorage.*` |

The optional `gui/WindowsGUI.cpp` uses the same classes as the console version. The module split follows our Phase-II proposal; everyone should understand the part they are presenting.

## Build

C++17 and its standard library are required. No external GUI packages or database are used.

**Windows PowerShell — console:**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude (Get-ChildItem src -Filter *.cpp | ForEach-Object FullName) -o EduTrackConsole.exe
.\EduTrackConsole.exe
```

**Windows PowerShell — desktop GUI (MinGW-w64):**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -municode -mwindows -Iinclude .\gui\WindowsGUI.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) -lcomctl32 -o EduTrackGUI.exe
.\EduTrackGUI.exe
```

For Linux/macOS console builds:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o edutrack
./edutrack
```

Run the app from the same working folder each time to find `students.txt`. Close the console before using the GUI, and vice versa, so they cannot overwrite one another's saved changes.

## Using the Windows GUI

The window has four sections, chosen from the left sidebar or the **View** menu:

- **Students:** add, update, delete and find student records. The search box lets you choose **Linear** or **Binary** search; these are algorithms taught in Data Structures. Use **Sort by** to view class rank, roll-number order or total marks. Click a table row to fill the form.
- **Request Queue:** use the same student form to queue add/update/delete requests, view the waiting list, and process requests in FIFO order. Only successfully applied changes can be undone.
- **Reports:** show one student's marks/grade/rank or the class and subject averages. Reports are refreshed on request after records change.
- **Grading:** view both grading schemes and their limitations. Selecting a different scheme changes displayed grades, not marks.

The **File** menu contains Save, Reload and Exit. The **Edit** menu contains Undo, Clear form and Find. Help explains the controls. The window can be minimized, maximized and resized (within a practical minimum size). Keyboard shortcuts: **Ctrl+S** Save, **Ctrl+Z** Undo, **Ctrl+F** Find, **F1** Help.

Records are stored locally. There is no login or online data transfer. Queue and undo history stay in memory; only student records are saved. Back up `students.txt` before deliberately testing corrupt-file recovery.


## Grading criteria

The default display uses a **GEU-published distance-learning reference**, not a verified grading regulation for our B.Tech batch. The university reference also discusses moderated grade boundaries. Here, the individual student's *average of equally weighted subject marks* is used to illustrate a letter grade; an actual university course grade or SGPA may be determined differently.

| Percentage | Illustrative grade | Points | Published rating |
| --- | --- | --- | --- |
| Above 85 | O | 10 | Outstanding |
| 75 to 85 | A+ | 9 | Excellent |
| 65 to below 75 | A | 8 | Very Good |
| 55 to below 65 | B+ | 7 | Good |
| 50 to below 55 | B | 6 | Above Average |
| 45 to below 50 | C | 5 | Average |
| 40 to below 45 | P | 4 | Pass |
| Below 40 | F | 0 | Fail |

The source leaves **exactly 85%** unspecified (`>85` is O, while the printed A+ range ends at 84). EduTrack treats 85 as A+ *temporarily*, rather than claiming that GEU has published a definitive rule for that boundary. The continuous boundaries between other published whole-number bands are also an application convention; results shown here are examples, not official university records.

**Source:** [Graphic Era (Deemed to be University), Programme Project Report, distance-learning grading table](https://files.geu.ac.in/odl/CIQA%202024/ppr.pdf). Confirm the current B.Tech-specific grading rules with your mentor before final submission.

The original EduTrack grading scheme is still selectable: A 90+, B 80+, C 70+, D 60+, F below 60. This is a project-only fallback; it has no grade points. Use the dropdown in the GUI or menu option **20** in the console to switch schemes. The choice is for the current session; the selected scheme does not alter student marks.

Performance bands are **project-defined**, not official GEU ratings:

| Scheme | Excellent | Good | Average | Needs Improvement |
| --- | --- | --- | --- | --- |
| GEU reference grouping | 75+ | 55 to below 75 | 45 to below 55 | Below 45 |
| Original project grouping | 85+ | 70 to below 85 | 50 to below 70 | Below 50 |

For example, a student with `360/500 = 72%` displays **A (8 points, Very Good)** using the GEU reference, while the original project scheme displays **C**. Both examples show the project performance band **Good**.

## Notes about subject reports and storage

Every subject is out of 100 and counted equally. The current data model stores subject marks by **position**, not by subject name: enter the same subjects in the same order for all students. If subject counts vary, only students with that subject position contribute to its average, and the report displays a warning. The program doesn't currently store custom subject titles.

Records are saved to `students.txt`. Temporary `students.txt.tmp` and `students.txt.bak` files can be created while saving. If these remain after an interrupted save, inspect them before restarting and do not discard a backup without checking it. Loading rejects invalid records rather than silently replacing the current list. Undo history and queued requests are not saved after exit.

## Current checks and limitations

The console application builds using C++17 on GCC and Clang and its data functions can be tested independently. The Win32 GUI source must also be compiled and tested on Windows/MinGW before we use it for the final demonstration. This project is meant for small student datasets; insertion sort and full undo snapshots are not designed for large institutional records.

## Before the Phase-II demo

- Confirm grading rules and subject order with the mentor; the GEU reference is not a verified B.Tech grading policy.
- Enter two example students, test sorting, both searches, undo, queue and reports.
- Save, close and reopen; verify the same marks and records load.
- Test the GUI on the Windows demonstration laptop and keep the console version as a fallback.
- Replace the placeholder repository/commit entries and old progress figures in the Phase-II report with accurate information.

## Phase III development (Stage 3C console checkpoint)

This branch adds the **Academic Improvement Simulator**, named academic groups, and
rank simulation for comparable classmates. It is intentionally separate from the
Phase-II `main` branch. The original console and Win32 GUI remain available.

### Console options added

| Menu | Function |
| --- | --- |
| 21 | Create an academic group with an ID, branch, semester, cohort and ordered named subjects |
| 22 | Assign an existing student to a group (exact branch/semester/subject count required) |
| 23 | Remove a student's group assignment |
| 24 | List groups, subject codes/names and assigned student counts |
| 25 | Preview a changed mark, grade and **rank within the assigned group** |

To compare students, first add their records, then create a group and explicitly
assign at least two classmates to it. The group ID and subject codes must use
uppercase letters, digits, hyphens or underscores. All subject positions must refer
to the same subjects. Cohort is the admission year (2000–2100).

A what-if simulation changes **nothing** in real records or files. It builds a
separate temporary list of assigned students from the **same group**, compares
percentage, then total marks and roll number (the existing rank tie-breakers),
and prints current and projected group ranks. Missing assignments, invalid subject
numbers and groups with fewer than two assigned students are rejected.

### Files and safety

`students.txt` is still `EDUTRACK 1`, unchanged from Phase II. Academic groups are
saved in `academic_groups.txt` (`EDUTRACK GROUPS 1`). The original Phase-II
application can continue to read `students.txt` without understanding groups.

Group changes save metadata immediately, after any unsaved student records have
been saved. In the Stage 3C console, edits, deletes, undo and queued operations
are checked **before** committing; any operation that would invalidate a group
assignment is blocked. A blocked queued request remains queued for later.
Unassign that student using option 23 before changing their name, branch,
semester, subject count or deleting the student. A marks-only update that keeps
identity and subject count is allowed.

Reload uses temporary objects and accepts the replacement only if **both** the
student file and the academic group file are valid. Corrupt or stale assignments
will not silently replace the active session. Existing `.tmp` or `.bak` files
must be inspected before restarting. Do not delete an old backup blindly.

**Known limitations:** This is **not** an atomic two-file transaction or a
multi-user database. The legacy Windows GUI is not yet connected to academic
group metadata and can make edits that invalidate those assignments; run only one
EduTrack application at a time, maintain backups, and use the Stage 3C console
for group-aware editing. Student identity is still based on roll number and name,
not a permanent immutable ID. The old all-class ranking and subject reports
remain as Phase-II features and are **not** equivalent to group-scoped analysis.

### Compile and try Stage 3C

Build the console with the same C++17 command in **Build** above; it automatically
includes the Stage 3A–3C `.cpp` files. On a clean folder:

1. Add two students in the same branch and semester with marks in the same subject order.
2. Save records (18); create a group (21) with the same number of named subjects.
3. Assign both students (22); show group details (24).
4. Simulate a mark change (25). Check rank and percentage; find the student again
   and verify the real marks did not change.
5. Try deleting an assigned student: it must be blocked. Unassign first (23)
   and then retry if deletion is intended.
6. Exit, reopen, and verify the group and assignments loaded successfully.

This checkpoint's console and academic modules passed strict GCC and Clang
compilation, sanitizer-level module checks and scripted save/reload tests on Linux.
The **Win32 GUI additions for Phase III are still pending** and must be built
and visually checked on Windows before use. Keep Phase-II `main` as the fallback.

## Phase III Stage 3D — Academic Insights (console)

The new **option 26, View group-based academic insights**, reads the selected
student's existing marks and compares each named subject with **other students
explicitly assigned to the same valid academic group**. It requires at least
one other comparable student. No group is inferred from branch or semester
alone.

For each subject, EduTrack prints the mark, the average for the *other*
students, and the difference in marks (`student mark - peer average`). It
identifies the largest negative gap and largest positive gap, if any. Equal
gaps are reported in the original subject order. It also displays the
student's percentage, the other students' average percentage and the current
group rank using the existing sorting rules. Unrelated or unassigned students
are excluded. When fewer than three other students are available, the report
warns that the sample is small.

These are **descriptive calculations only**. A low mark or negative group gap
cannot establish why somebody performed differently, and the report does not
predict future results or automatically assign a study plan. All insights are
read-only; existing student and academic-group file formats are unchanged.

### Example

If a student scores 40 in Mathematics and the only comparable peer scores 80,
the subject gap is `40 - 80 = -40` marks. This is described as 40 marks below
that peer's average, **not** a prediction that 40 marks can be recovered.

**Tested:** strict GCC/Clang console compilation; AddressSanitizer and
UndefinedBehaviorSanitizer checks for peer exclusion, group isolation, ties,
stale assignments and unchanged records; console group setup, insight
printing, rank simulation and save/restart. The Stage 3D insight controls are
**not yet connected to the Windows GUI**. The Phase-II `main` branch remains
the fallback while Phase-III branches are under review.

## Phase III Stage 3E — Improvement Goal Planner (console)

**Menu option 27** computes a hypothetical plan for one student. It works with
or without an academic-group assignment; assigned students see their verified
subject names, while others see numbered subjects. Choose either:

- **Percentage target:** an exact percentage from 0 to 100 with at most two
  decimal places (for example `75.25`). The smallest integer total that meets
  that percentage is calculated using integer arithmetic. The displayed
  percentage is rounded for readability, but the decision is **not** rounded.
- **Letter-grade target:** one of the letter grades available under the currently
  selected *illustrative* grading scheme (option 20). The F grade is not offered
  as an improvement target. The reference O threshold is strictly **above 85%**;
  for example, with five subjects O requires at least **426/500**, not 425.

You can accept a limit of 100 for each subject or enter **hypothetical subject
ceilings** between the student's current marks and 100. If the goal exceeds the
sum of these ceilings, EduTrack clearly reports that it is **unreachable under
those ceilings**, shows the maximum obtainable total and the shortfall, and
prints an illustration using the chosen ceilings. If the current total already
meets the target, no mark increase is proposed.

For reachable targets, the planner proposes one valid distribution of the
minimum additional marks. Its simple, deterministic rule is to add marks to the
**currently lowest hypothetical subject** with remaining capacity, breaking
same-mark ties by original subject order. This is a mathematical illustration,
**not** an optimization for learning time, academic difficulty, predicted
performance, future exams or study effort. All subjects are equally weighted
and scored out of 100, consistent with the existing student model. There is no
SGPA or officially verified B.Tech course grading calculation.

**Example:** marks `80, 45, 75, 70, 60` sum to `330/500 = 66%`. A target of
`75%` requires at least `375` marks: an additional **45** marks overall. A
`75.01%` target requires `376/500` (not `375`). Both results are simulations;
no real mark, undo history, queue, `students.txt` or `academic_groups.txt` is
changed when using option 27.

**Validation:** strict GCC and Clang C++17 builds; a sanitizer-backed suite
covering exact percentage thresholds, impossible ceilings, grades under both
scales, the special O boundary, 0/100 limits, invalid data and read-only console
interaction. A native Win32 GUI implementation of Stage 3E is still pending.
The Phase-II `main` branch remains the fallback; earlier Phase-III checkpoints
are under review.

## Phase III desktop preview — Stage 3F1 (read-only)

Stage 3F1 adds a **separate** desktop program in `gui/Phase3GUI.cpp`. This is the
new academic workspace with **Dashboard**, **Student profiles**, **Academic insights**,
**What-if simulator**, **Goal planner**, **Academic groups** and **Help** navigation.
It uses the same Stage 3C–3E calculations as the console. It is intentionally
**read-only**: it does not create/edit/delete records or assignments and does not
write `students.txt` or `academic_groups.txt`. This prevents the older GUI's
uncoordinated record edits from invalidating the new academic-group sidecar.

**Windows PowerShell — Phase III preview (MinGW-w64):**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -municode -mwindows -Iinclude .\gui\Phase3GUI.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) -lcomctl32 -o EduTrackPhase3.exe
.\EduTrackPhase3.exe
```

Compile **only** `Phase3GUI.cpp` for this program; `gui/WindowsGUI.cpp` is the
unchanged Phase-II desktop application with a separate `wWinMain` entry point.
The Phase-III console remains the safe place to create academic groups, assign
students, and edit records (menu options 21–27). Close the console before
opening/reloading the desktop. The preview reads files from the **current
working directory**; launch it in the same project/data directory.

**GUI workflow:** Choose a student in the top selector; review Student profiles
and Academic insights; use What-if simulator to adjust a single hypothetical
subject mark and optionally compare the group rank; use Goal planner to enter
an exact target percentage (such as `75.25`) or grade (`A+`) and optional
per-subject ceiling marks in stored subject order. Select Reload files after
closing the console to import saved changes. No action in this preview changes
student records. If no comparable group exists, the simulator still reports
marks/grade changes but correctly omits projected rank, and the Insight page
explains that peers are required.

**Safety and validation:** The program blocks load when `.tmp` or `.bak`
recovery files are present, rejects corrupt or inconsistent academic metadata,
and loads into temporary objects before replacing the displayed data. It does
not silently discard assignments. Missing files are treated as empty datasets.
GCC and Clang portable-console checks and independent analytic tests can be
run on Linux/macOS, but this is **not** a native Windows GUI test. Before a
university demonstration, compile the preview on Windows and inspect each
page at default/high DPI and after maximize/restore; verify the text does not
clip or overprint. Stage 3F2 will address safe group-editing controls and the
remaining Windows visual tests. The default GitHub `main` and older Phase-II
GUI remain the fallback.


## Phase III Stage 3F2 — Phase-II-style hybrid desktop (review build)

The new **`gui/Phase3HybridGUI.cpp`** is an optional third Windows entry point.
It borrows the Phase-II desktop's familiar sidebar, native File/Edit/View/Help
menus and Students table, while keeping the Stage-III Dashboard, Insights,
What-if Simulator, Goal Planner and Academic Groups.

**Windows PowerShell, from the `EduTrack` directory (MinGW-w64):**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -municode -mwindows -Iinclude .\gui\Phase3HybridGUI.cpp (Get-ChildItem src -Filter *.cpp | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName) -lcomctl32 -o EduTrackHybrid.exe
.\EduTrackHybrid.exe
```

Compile **only one GUI source** at a time: `WindowsGUI.cpp` (Phase II),
`Phase3GUI.cpp` (working read-only Stage 3F1 preview), or
`Phase3HybridGUI.cpp` (Stage 3F2 hybrid). The console build is unchanged.

In the hybrid GUI:

- **Students** displays a sortable student table with a profile pane. Find by
  roll number using either linear or binary search. It does **not** edit marks.
- **Request Queue** explains the existing FIFO/undo workflow; queue processing,
  record CRUD and undo are still available in the **Phase-III console only**.
- **Reports** displays class statistics and a selected student report; the
  overall class average is not a comparable-group average.
- **Grading** switches between two illustrative scales. This affects displayed
  grades, simulations and goals, never stored marks. The GEU reference is not
  confirmed as the actual B.Tech grading policy.
- **Academic Groups** displays subject definitions and lets you **assign or
  unassign a selected student to an existing group**. A confirmation is required.
  The UI checks group compatibility and validates that neither saved file has
  changed since it was loaded. A successful action writes **only**
  `academic_groups.txt` with temporary/backup recovery safeguards. New group
  definitions still require the console.
- **Academic Insights, What-if Simulator, Goal Planner** use the existing
  read-only Phase-III engines. A hypothetical result never changes a record.
- Native menu shortcuts: Ctrl+R reload, Ctrl+F find, F1 help.

**Important safety limitations:**

- This is **not an atomic two-file database**. There is no shared lock between
  this GUI and the console/other programs. Close one before launching the other;
  back up both `students.txt` and `academic_groups.txt` before editing groups.
- If files changed externally or `.tmp`/`.bak` recovery files are present,
  group editing is refused. Inspect recovery files and use Reload before retrying.
- Student CRUD/queue operations cannot yet be performed from the hybrid GUI;
  they require coordinated mutation safeguards and remain a later GUI milestone.
- Stage 3F2 passed portable C++ integration/sanitizer checks and static GUI
  layout/source checks. **It has not yet been compiled or visually exercised on
  Windows in this environment.** Test on your Windows laptop before merging the
  review PR. The already-tested Stage 3F1 GUI remains intact as a fallback.
