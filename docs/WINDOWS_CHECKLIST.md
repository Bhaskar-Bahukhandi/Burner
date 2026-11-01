# EduTrack — Windows verification and handover checklist

This checklist is deliberately **unchecked** until someone runs the compiled application on the intended Windows machine. Passing portable GCC/Clang tests is not proof that the native Windows GUI works.

## Before testing

- [ ] Extract the newest ZIP into a **new** folder; do not mix source files from older versions.
- [ ] Install the matching MSYS2 UCRT64 GCC and SQLite development packages described in `README.md`.
- [ ] Confirm PowerShell can run `build_windows.ps1`.
- [ ] Keep real student data out of the demonstration database.
- [ ] Close any other EduTrack console or GUI instances before changing records.

## Build and automated tests

Run in PowerShell from the project root:

```powershell
.\build_windows.ps1 -Target All
.\run_windows.ps1 -Target Tests
.\run_windows.ps1 -Target GUI
```

Record whether every build succeeded and keep the first complete compiler error if a build fails.

- [ ] GUI, console, importer, maintenance utility and test programs compile without errors.
- [ ] All six test executables exit successfully (including the Windows dialog smoke test).
- [ ] `EduTrack.exe` opens and uses the intended `edutrack.db` file.
- [ ] Starting with an empty directory creates a new database only when no legacy data is waiting to be imported.

## GUI acceptance using fictional students

Use the sample group and three students in `docs/PROJECT_AND_DEMO.md`.

- [ ] Sidebar, menu bar and student table look readable at normal size, maximized and restored.
- [ ] Text and dialog controls remain usable with Windows display scaling of 100% and 125% (if available).
- [ ] Add, edit and delete work for valid unassigned records; duplicate roll numbers and invalid marks are rejected.
- [ ] The student form shows separate mark fields, and assigned students show the correct subject names and count.
- [ ] Academic-group creation accepts a valid group and rejects repeated subject codes, invalid semester and bad subject counts.
- [ ] Assigned-student edits that would break group consistency are blocked.
- [ ] Student search, sorting, table selection and profile agree on the selected roll number.
- [ ] Tied 80% students show the same academic rank (1, 1, 3 with the example data).
- [ ] The Reports chart shows only the selected verified group's named subject averages.
- [ ] Insights excludes the selected student from peer means; an ECE student's first mark is not averaged into a CSE subject.
- [ ] What-if Simulator and Goal Planner show alternative results without changing saved marks.
- [ ] A valid queued request is processed in FIFO order; a group-blocked request stays pending.
- [ ] Undo restores an eligible operation; reloading clears session-only queue/undo state.
- [ ] Save/restart preserves committed student records and group assignments.
- [ ] A second editor's stale data is rejected rather than silently overwriting newer database changes.

## Database and privacy checks

From the same PowerShell project directory, with a closed application, run:

```powershell
.\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'edutrack.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('backup', 'edutrack.db', 'demo-backup.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('restore', 'demo-backup.db', 'demo-restored.db')
.\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'demo-restored.db')
```

- [ ] Verify reports a valid database without orphan foreign keys.
- [ ] Backup retains group definitions and all marks; backup does not modify the source database.
- [ ] Restore creates a new file rather than replacing the active database.
- [ ] Backup/restore refuses an existing destination and unsafe leftover SQLite sidecar files.
- [ ] An attempted migration refuses a pre-existing destination and retains the source text files.
- [ ] No actual student data, SQLite database, compiler binary or screenshots with private records are included in the source ZIP.

SQLite is not encrypted by default. Do not describe an unencrypted database as secure against someone who can access its files.

## Team handover

- [ ] Each team member can explain their module's inputs, outputs, validation and one edge case.
- [ ] The team can explain linear versus binary search, merge sort, competition ties, stack undo, FIFO queue and SQLite transactions.
- [ ] The team has rehearsed the fictional-data demonstration without editing real student records.
- [ ] Any speed comparison with a spreadsheet uses actually measured results, not estimates presented as observations.
- [ ] Only the latest source ZIP and any genuinely required university submission material are shared.

**Outcome**

Build tested on (Windows version / date): ________________________

Compiler/toolchain used: ______________________________________

Tests passed / failed: ________________________________________

Outstanding defects: _________________________________________

Reviewed by: _________________________________________________

## Modal dialogs and first-run UX

- [ ] `run_windows.ps1 -Target Tests` runs all six tests, including dialog creation; all return exit code 0.
- [ ] An empty dashboard explains the first two actions and its Add student / Create academic group shortcuts open populated dialogs.
- [ ] Create academic group shows Group ID, Branch, Semester, Cohort, ordered subjects and Save/Cancel.
- [ ] New student shows roll, name, branch, semester and the Edit subject marks button; nested subject inputs show and accept 0 and 100.
- [ ] No blank forms, failed child-control messages, or invisible controls.
- [ ] Cancel and Escape close dialogs without saving changes.
- [ ] Student table headings are readable; at minimum width it uses horizontal scrolling instead of truncating all columns.
- [ ] Edit/Delete, assign/unassign, simulation and goal buttons are disabled when prerequisites are missing.
- [ ] Goal Planner Set subject limits opens labelled fields, rejects ceilings below current marks, and Clear limits restores the default.
- [ ] Resize, maximize, restore and revisit all ten pages with empty and populated databases.
