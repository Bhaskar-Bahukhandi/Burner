# EduTrack release test plan

Use a **disposable database containing fictional students only**. Do not perform destructive tests on real records.

Record **Pass / Fail / Blocked**, Windows version, compiler path, time, and any supporting screenshots for each test. A compiled program or passing CI job is not proof of every GUI behavior.

## Fast commands

```powershell
.\build_windows.ps1 -Target All
.\run_windows.ps1 -Target Tests
.\run_windows.ps1 -Target GUI
.\run_windows.ps1 -Target Maintenance -Arguments @('verify', 'edutrack.db')
```

The six test executables cover core algorithms, validation boundaries, SQLite storage, workflow, maintenance and native dialog creation. The CI workflow runs five non-interactive programs. Run the interactive GUI smoke test locally.

## Build and launch

- [ ] **T001 — Clean checkout:** Extract the source ZIP into a new directory. **Expected:** All source directories exist; no executable or database is preloaded.
- [ ] **T002 — Compiler available:** Run g++ --version using the documented UCRT64 installation. **Expected:** Compiler identifies the configured UCRT64 toolchain.
- [ ] **T003 — SQLite available:** Check sqlite3.h and the import library in the same UCRT64 installation. **Expected:** Both are present.
- [ ] **T004 — Build all:** Run .\build_windows.ps1 -Target All. **Expected:** All application, utility, and six test executables build.
- [ ] **T005 — Run tests:** Run .\run_windows.ps1 -Target Tests. **Expected:** Six executables exit 0; GUI smoke closes its dialogs.
- [ ] **T006 — Missing DLL:** Temporarily omit the runtime PATH, then use the supported runner. **Expected:** Runner starts with the configured UCRT64 DLLs.
- [ ] **T007 — Missing compiler:** Run build helper with an intentionally invalid -Ucrt64 path. **Expected:** A clear error appears; no false success message.
- [ ] **T008 — Missing executable:** Delete only an expendable compiled copy and run that target. **Expected:** Runner requests a rebuild instead of claiming success.
- [ ] **T009 — Alternate installation:** Supply -Ucrt64 pointing to the real non-default installation. **Expected:** Build and run use the requested installation.
- [ ] **T010 — Current directory:** Invoke the runner from another PowerShell directory. **Expected:** The program still resolves the project database consistently.

## Dashboard and navigation

- [ ] **T011 — First launch:** Start with no database or legacy records. **Expected:** An empty database is initialized and the Dashboard explains how to start.
- [ ] **T012 — First student shortcut:** Use the Dashboard add-student shortcut. **Expected:** A populated student form opens.
- [ ] **T013 — First group shortcut:** Use the Dashboard create-group shortcut. **Expected:** A populated group form opens.
- [ ] **T014 — Sidebar navigation:** Open every sidebar page once. **Expected:** Each page displays its own controls and correct heading.
- [ ] **T015 — Menu navigation:** Use each enabled application menu item. **Expected:** The requested page or action is triggered exactly once.
- [ ] **T016 — Return to dashboard:** Navigate away and back. **Expected:** Student count and other summaries remain consistent.
- [ ] **T017 — Resize narrow:** Resize the main window to its minimum. **Expected:** No essential input disappears; table scrolling is available.
- [ ] **T018 — Maximize and restore:** Maximize, restore, and move the window. **Expected:** Content stays readable; selections are not lost.
- [ ] **T019 — Empty state:** Open Reports and Planner with an empty database. **Expected:** Helpful empty states appear; actions requiring records are unavailable.
- [ ] **T020 — Rapid switching:** Switch pages repeatedly while data is populated. **Expected:** No crash, blank panel, or stale selected student.

## Student records

- [ ] **T021 — First valid student:** Add roll 101, name Asha, CSE, semester 3, two marks. **Expected:** Student appears once in the table and profile.
- [ ] **T022 — Second valid student:** Add roll 102, name Ravi, same branch and semester. **Expected:** Both records appear; rolls remain ordered.
- [ ] **T023 — Duplicate roll:** Try adding roll 101 again. **Expected:** Save is rejected without changing the first record.
- [ ] **T024 — Roll zero:** Enter roll number 0. **Expected:** Input is rejected.
- [ ] **T025 — Negative roll:** Enter a negative roll number. **Expected:** Input is rejected.
- [ ] **T026 — Large roll:** Enter roll number 1000000000. **Expected:** Out-of-range input is rejected.
- [ ] **T027 — Blank name:** Leave the student name empty or spaces only. **Expected:** Input is rejected.
- [ ] **T028 — Multiline name:** Paste a newline into the name field. **Expected:** Invalid input is rejected.
- [ ] **T029 — Invalid semester:** Try semester 0 and 13. **Expected:** Both values are rejected.
- [ ] **T030 — Valid boundaries:** Add valid fictional records with semesters 1 and 12. **Expected:** Records save correctly.
- [ ] **T031 — Edit marks:** Change roll 101 subject marks then save. **Expected:** Only the selected record changes.
- [ ] **T032 — Edit identity:** Attempt to change a locked roll during an edit. **Expected:** Existing roll remains unchanged.
- [ ] **T033 — Delete confirmation:** Select an unassigned student and request deletion. **Expected:** A confirmation appears before deletion.
- [ ] **T034 — Cancel delete:** Cancel the deletion. **Expected:** Record remains unchanged.
- [ ] **T035 — Delete record:** Confirm deletion for an unassigned student. **Expected:** Record disappears; remaining records stay intact.
- [ ] **T036 — No selection:** Clear selection and inspect edit/delete actions. **Expected:** Actions that require a selection are disabled or safely rejected.

## Subject marks

- [ ] **T037 — Minimum mark:** Enter mark 0. **Expected:** Accepted as a valid mark.
- [ ] **T038 — Maximum mark:** Enter mark 100. **Expected:** Accepted as a valid mark.
- [ ] **T039 — Negative mark:** Try mark -1. **Expected:** Rejected.
- [ ] **T040 — Over maximum:** Try mark 101. **Expected:** Rejected.
- [ ] **T041 — Decimal mark:** Try mark 73.5. **Expected:** Rejected; only whole numbers accepted.
- [ ] **T042 — Non-numeric mark:** Try abc in a mark field. **Expected:** Rejected.
- [ ] **T043 — One subject:** Set an unassigned student to one subject. **Expected:** Saved mark count is one.
- [ ] **T044 — Twenty subjects:** Set an unassigned student to twenty subjects. **Expected:** All twenty are retained.
- [ ] **T045 — Twenty-one subjects:** Attempt to set twenty-one subjects. **Expected:** Rejected at 20-subject limit.
- [ ] **T046 — Cancel marks:** Edit several marks then choose Cancel/Escape. **Expected:** The student retains the original marks.

## Academic groups

- [ ] **T047 — Create CSE group:** Create group CS25, branch CSE, semester 3, cohort 2025, ordered DS and MA subjects. **Expected:** Group is saved with exactly two subject definitions.
- [ ] **T048 — Assign valid student:** Assign matching two-subject CSE student. **Expected:** Student shows the group and subject names.
- [ ] **T049 — Wrong branch:** Attempt to assign an ECE student to CS25. **Expected:** Assignment is rejected.
- [ ] **T050 — Wrong semester:** Attempt to assign a CSE semester-4 student to CS25. **Expected:** Assignment is rejected.
- [ ] **T051 — Wrong subject count:** Attempt to assign a three-subject student to CS25. **Expected:** Assignment is rejected.
- [ ] **T052 — Duplicate group ID:** Create another CS25 group. **Expected:** Duplicate ID rejected.
- [ ] **T053 — Duplicate subject code:** Use DS twice in one group. **Expected:** Invalid definition rejected.
- [ ] **T054 — Invalid subject code:** Use lowercase or punctuation in a subject code. **Expected:** Invalid code rejected.
- [ ] **T055 — Invalid cohort:** Use year 1999 or 2101. **Expected:** Invalid cohort rejected.
- [ ] **T056 — Invalid group semester:** Use semester 0 or 13. **Expected:** Invalid semester rejected.
- [ ] **T057 — Edit assigned student:** Try an edit that changes branch, semester, or mark count. **Expected:** Inconsistent edit is blocked.
- [ ] **T058 — Delete assigned student:** Attempt deletion while student is assigned. **Expected:** Safety rule blocks the operation.
- [ ] **T059 — Unassign then delete:** Unassign and retry deletion. **Expected:** Deletion now succeeds.
- [ ] **T060 — Other department:** Create matching ECE group with unrelated subject codes. **Expected:** ECE subject names never appear in CSE results.

## Search, sort and rank

- [ ] **T061 — Exact roll search:** Search for a saved roll number. **Expected:** Correct student selected.
- [ ] **T062 — Missing roll search:** Search for an absent roll number. **Expected:** Not-found status without selecting the wrong student.
- [ ] **T063 — Linear search:** Use the linear search mode. **Expected:** Correct record is returned.
- [ ] **T064 — Binary search:** Use the binary search mode. **Expected:** Correct record is returned.
- [ ] **T065 — Search equivalence:** Search the same roll with both modes. **Expected:** Results are identical.
- [ ] **T066 — Ascending roll:** Sort by roll number. **Expected:** Rolls appear in numerical order.
- [ ] **T067 — Sort by total:** Sort students with different totals. **Expected:** Highest total appears first.
- [ ] **T068 — Sort by percentage:** Compare students with different subject counts. **Expected:** Percentages, not raw totals, determine order.
- [ ] **T069 — Tied rank:** Create two students with 80% and one with 70%. **Expected:** Competition ranks display 1, 1, 3.
- [ ] **T070 — Table/profile sync:** Select three different rows in sequence. **Expected:** Selected roll and profile always match.

## Reports and academic insights

- [ ] **T071 — Named subject report:** Open report for CS25 after assigning two students. **Expected:** DS and MA names are visible in report.
- [ ] **T072 — No group report:** Request a report for a group with no assignments. **Expected:** Empty report is clear; no invented averages.
- [ ] **T073 — Other group report:** Select an ECE group with separate subjects. **Expected:** Only its own subject names and members appear.
- [ ] **T074 — Extreme marks:** Use one student with 0 and another with 100. **Expected:** Average of the shared subject is 50.
- [ ] **T075 — Student count:** Assign and unassign a member. **Expected:** Report counts update correctly.
- [ ] **T076 — Outlier subject:** Change one committed mark then reopen report. **Expected:** Mean, minimum and maximum update.
- [ ] **T077 — Peer exclusion:** Inspect peer insights for a selected student. **Expected:** Selected student is not included in peer mean.
- [ ] **T078 — Single member:** Inspect insights for a group containing only one student. **Expected:** No fabricated peer comparison is presented.
- [ ] **T079 — Stale selection:** Switch selected student after calculating a report. **Expected:** Displayed student context updates correctly.
- [ ] **T080 — No mixed subjects:** Use CSE DS and ECE Circuits in position 1. **Expected:** Their subject statistics remain separate.

## Queue and undo

- [ ] **T081 — Queue add:** Queue a valid add without processing. **Expected:** Record is not committed immediately.
- [ ] **T082 — Queue update:** Queue a valid update. **Expected:** Pending queue shows the requested operation.
- [ ] **T083 — Queue delete:** Queue a delete. **Expected:** Pending queue shows the requested operation.
- [ ] **T084 — FIFO order:** Queue add, update and delete for a controlled student. **Expected:** Process-all follows insertion order.
- [ ] **T085 — Process next:** Queue two changes and process one. **Expected:** Exactly one request is consumed.
- [ ] **T086 — Undo add:** Apply an add, then undo. **Expected:** Added record disappears.
- [ ] **T087 — Undo update:** Apply an update, then undo. **Expected:** Earlier values are restored.
- [ ] **T088 — Undo delete:** Delete an unassigned record, then undo. **Expected:** Deleted record is restored.
- [ ] **T089 — Blocked queued deletion:** Queue deletion of an assigned student. **Expected:** Request remains pending and student remains intact.
- [ ] **T090 — Reload behavior:** Reload the program after a committed edit. **Expected:** Committed records persist; session queue and undo clear.

## What-if simulator and goal planner

- [ ] **T091 — No selected student:** Open simulator without a selection. **Expected:** Run action is disabled or safely rejected.
- [ ] **T092 — Valid proposed mark:** Simulate subject mark change 60 to 80. **Expected:** Proposed results differ; saved mark stays 60.
- [ ] **T093 — Decreasing mark:** Simulate reducing a mark. **Expected:** Hypothetical rank reflects the decrease.
- [ ] **T094 — Zero proposed mark:** Try proposed mark 0. **Expected:** Valid scenario is calculated.
- [ ] **T095 — Above maximum:** Try proposed mark 101. **Expected:** Rejected without changing stored data.
- [ ] **T096 — Target already reached:** Set a target below the current percentage. **Expected:** Planner explains no additional marks are needed.
- [ ] **T097 — Reachable target:** Set a modest improvement target. **Expected:** Required total and suggested allocation are consistent.
- [ ] **T098 — Unreachable target:** Set an impossible target with tight subject ceilings. **Expected:** Planner reports that it cannot be reached.
- [ ] **T099 — Set limits:** Set ceilings that are at least current marks. **Expected:** Limits are retained for the scenario.
- [ ] **T100 — Clear limits:** Clear previously set limits. **Expected:** Default scenario behavior returns.

## Database, import, backup and restore

- [ ] **T101 — Persistence:** Add a record, close the app, reopen. **Expected:** Record and marks remain.
- [ ] **T102 — Group persistence:** Create group and assignment, restart. **Expected:** Both remain.
- [ ] **T103 — Database verification:** Run maintenance verify on closed demo database. **Expected:** SQLite integrity and foreign-key checks pass.
- [ ] **T104 — Backup:** Run maintenance backup to a new filename. **Expected:** New verified database appears; original is unchanged.
- [ ] **T105 — Duplicate backup name:** Try backup to an existing file. **Expected:** Refused; existing file not overwritten.
- [ ] **T106 — Restore to new name:** Restore a verified backup to unused destination. **Expected:** New verified database created; live file untouched.
- [ ] **T107 — Restore collision:** Attempt restore to an existing database. **Expected:** Refused.
- [ ] **T108 — Corrupt database:** Verify a fake text file named bad.db. **Expected:** Verification fails clearly.
- [ ] **T109 — Stale WAL destination:** Create an abandoned destination-wal before backup. **Expected:** Backup refuses an unsafe destination.
- [ ] **T110 — Legacy import:** Import a valid legacy text export into unused destination. **Expected:** Rows and groups imported; originals unchanged.
- [ ] **T111 — Bad legacy import:** Import malformed legacy content. **Expected:** Rejected without publishing a partial database.
- [ ] **T112 — Import collision:** Repeat import to the same destination. **Expected:** Existing SQLite database remains intact.
- [ ] **T113 — Second editor conflict:** Open a stale snapshot and attempt conflicting change. **Expected:** Stale write is rejected.
- [ ] **T114 — Crash-recovery check:** Close the app and inspect database plus sidecar files. **Expected:** Recovery and verification work; do not manually delete live WAL files.

## UI, display and keyboard

- [ ] **T115 — Normal DPI:** Open on 100% Windows scale. **Expected:** Text, buttons and forms fit.
- [ ] **T116 — 125 percent DPI:** Open on 125% Windows scale. **Expected:** Text and forms remain usable.
- [ ] **T117 — Optional 150 percent:** If supported, open on 150% scale. **Expected:** No essential controls inaccessible.
- [ ] **T118 — Minimum window size:** Reduce application to allowed minimum. **Expected:** Navigation and table remain functional.
- [ ] **T119 — Long names:** Use a student with a long but valid name. **Expected:** Name stays legible or is clearly clipped without overlap.
- [ ] **T120 — Long subject labels:** Create a valid long subject name. **Expected:** Labels have clear fallbacks; no overlapping input boxes.
- [ ] **T121 — Keyboard Tab:** Tab through each student form. **Expected:** Focus reaches all required inputs and buttons.
- [ ] **T122 — Escape:** Press Escape while a form is open. **Expected:** Form closes without committing.
- [ ] **T123 — Cancel during marks:** Edit marks, choose Cancel, reopen. **Expected:** Original marks remain.
- [ ] **T124 — Repeated modal dialogs:** Open and cancel each editor several times. **Expected:** No blank dialogs, hidden inputs, or unresponsive main window.

## Privacy and release hygiene

- [ ] **T125 — No real records:** Inspect the source ZIP and test database. **Expected:** Only fictional names and non-private sample records.
- [ ] **T126 — No compiled files in source:** Inspect ZIP entries for exe, dll and o files. **Expected:** No compiled binaries included.
- [ ] **T127 — No database in source:** Inspect ZIP entries for db, sqlite and WAL files. **Expected:** No live student database included.
- [ ] **T128 — No abandoned branches in ZIP:** Inspect ZIP top-level entries. **Expected:** No phase folders, .git history, or generated artifacts.
- [ ] **T129 — README instructions:** Follow Windows installation and launch instructions. **Expected:** Instructions match scripts shipped.
- [ ] **T130 — Test instructions:** Follow test commands from README. **Expected:** All documented commands work on intended toolchain.
- [ ] **T131 — Team explanation:** Each student explains a core module and one edge case. **Expected:** Explanations match implementation.
- [ ] **T132 — Backup before upgrade:** Copy existing database to protected location before switching build. **Expected:** Rollback copy can be independently verified.
- [ ] **T133 — No false claims:** Review README and slides against application. **Expected:** No claims of production ERP, official grading, or predictive ML.
- [ ] **T134 — Final clean directory:** Zip only tracked source, test and documentation files. **Expected:** No machine-specific settings, log files or caches.

## Release gate

- [ ] Every automated test passes against the intended Windows toolchain.
- [ ] No data-loss, persistence, startup, crash, or GUI-blocker tests remain failed.
- [ ] All critical tests above have a recorded Pass result; blocked tests are explained, not treated as passes.
- [ ] The team has verified a database backup and documented rollback to the prior release.
- [ ] Project files, scripts and README match the exact revision packaged for submission.

Windows version: __________________  Date: __________________

Compiler and UCRT64 path: __________________

Passed: ______  Failed: ______  Blocked: ______  Not run: ______

Tester: __________________

Defect references and notes: ____________________________________________