# EduTrack — Project and demonstration guide

This is a speaking and handover guide for the team. Use your own words while presenting it. The sample records below are fictional; the comparison timings are deliberately left blank until measured.

## 1. The problem we are solving

A faculty member may need to find a student's record, edit marks, produce a class report, and explain which subjects a student could improve. A spreadsheet can store marks and perform calculations, but these tasks often need separate sorting, formulas and manual checks. A university ERP can have broader official functions, but this student project is a small, local tool for learning C++/DSA while combining these common workflows.

**Project aim:** Build a desktop application that manages student marks, applies basic data structures and algorithms, and generates explainable, group-specific academic summaries without changing records during a simulation.

**Primary user:** faculty member or academic coordinator. Students are the people whose results are analysed, but the current program does **not** provide student accounts or access control.

**What makes it useful in a demo:** a consistent roll-number lookup; a separate record-edit workflow; FIFO requests and session undo; academic groups with named subjects; SQL-backed data consistency; and a hypothetical goal planner. These are features to demonstrate, **not evidence of faster work until measured**.

**What it does not claim:** a replacement for the university ERP, official grades/SGPA, a machine-learning prediction, encrypted storage, multi-user access or guaranteed academic improvement.

## 2. Suggested team explanation

The allocations below are the project's module responsibilities. Each team member should explain the code they actually reviewed, understand how it integrates, and be ready to demonstrate one path through it.

| Team member | Main module to explain | Where to look |
| --- | --- | --- |
| Mohammad Rayyan Farooqui | Student management and application integration | `StudentManagement.*`, `gui/WindowsGUI.cpp` |
| Shiv Tyagi | Linear/binary search, merge sort and rankings | `RecordAlgorithms.*` |
| Bhaskar Bahukhandi | Stack-based undo, FIFO requests and guarded changes | `ChangeHistory.*`, `RequestQueue.*`, `GuardedRecordWorkflow.*` |
| Aditya Kukrety | Marks, grade/report calculations and storage | `AcademicProcessing.*`, `database/SqliteStorage.cpp` |

**Integration story:** the desktop form validates input, domain classes apply the business rules, and the SQLite storage code checks that the loaded data has not become stale before committing a transaction. The console uses the same core logic; the old text readers are present for one-time import.

## 3. Algorithms and realistic complexity

Let **n** be the number of loaded students and **q** the number of pending requests. These costs describe the relevant algorithm, not the whole GUI operation, file I/O or database transaction.

| Feature | Structure / algorithm | Typical time | Reason / trade-off |
| --- | --- | --- | --- |
| Find by roll (linear) | Scan student vector | O(n) | Easy baseline for the DSA comparison |
| Find by roll (binary) | Binary search on sorted vector | O(log n) | Records are kept in roll order after add/delete |
| Add or delete student | Sorted `std::vector` | O(n) | Finding the position is fast; shifting records takes linear time |
| Sort by total/percentage | Merge sort on pointers | O(n log n) | Sorting avoids copying entire Student objects; needs O(n) extra pointers |
| Produce competition ranks | Merge sort, then scan | O(n log n) | Equal percentages share the same rank; ties are compared exactly |
| Put request into queue | `std::queue` | O(1) | FIFO insertion, assuming standard amortized container behavior |
| Process next request | Queue front + guarded record edit | Includes O(n) record copying/updates | Group checks may reject an operation; blocked requests remain queued |
| Record an undo action | Vector stack of inverse actions | Amortized O(1) plus copy of one original Student | Lower history memory than a full-class snapshot; undo itself can shift vector records |
| Subject report for a group | Scan records and accumulate marks | O(n × s), s = subject count | Only verified members of that group are compared |

**Binary-search precondition:** do not feed it an arbitrary unsorted vector. The application uses the roll-ordered collection managed by `StudentManagement`.

**Ranking rule:** if two students have 80% each, both receive rank 1; if a third has 70%, their rank is 3. Roll number makes tied display order deterministic, not a different academic rank. Subject-specific comparison always requires a matching academic group.

**SQLite justification:** foreign keys, CHECK constraints, parameterized queries and transactions keep related records consistent. SQLite is embedded and needs no server for one-PC operation. It is not automatically encrypted, and this design is not a multi-user ERP.

## 4. Fictional records for a 6–8 minute demo

Create the group `CSE-2026-S3`: branch `CSE`, semester `3`, cohort `2026`, with ordered subjects `DS` (Data Structures), `MATH` (Mathematics), `OOP` (Object-Oriented Programming).

| Roll | Fictional name | Branch / semester | DS | MATH | OOP | Percentage |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 101 | Aarav | CSE / 3 | 80 | 75 | 85 | 80% |
| 102 | Neha | CSE / 3 | 78 | 82 | 80 | 80% |
| 103 | Rohan | CSE / 3 | 70 | 70 | 70 | 70% |

Add and assign all three students to the group. Their competition ranks are **1, 1, 3**. For roll 101, the other two students have peer subject averages of **74 in DS, 76 in MATH and 75 in OOP**. The target student is excluded from those peer averages.

For a second, separate group, you may define `ECE-2026-S3` with branch `ECE` and its own subjects. It must **not** be averaged into the CSE subject report, even when some marks happen to occupy the same numeric position.

Suggested walk-through:

1. Show the Dashboard and explain that the application is aimed at a faculty workflow.
2. Open Academic Groups, create `CSE-2026-S3` with the three ordered subject definitions.
3. Add the three fictional students under Students; assign them to the group and show their named mark fields.
4. Search for roll 102, sort the table, and explain the precondition for binary search.
5. Open Reports and Insights for roll 101. Show the group chart, tied ranks and peer-only averages.
6. Open What-if Simulator: test increasing roll 101's DS mark from 80 to 90. Explain that the database has not changed.
7. Open Goal Planner: an 85% target for roll 101 requires a total of **255/300**, which is **15 additional marks** from the original 240/300. The proposed distribution is only a hypothetical example.
8. Edit one actual mark, demonstrate Undo; optionally queue a change and Process Next. Restart the application to confirm saved changes, and explain that queue/undo history is session-only.

Before a live demonstration, take a verified backup of the fictional database. Use only data created for the demonstration, not actual student records.

## 5. Measuring the workflow instead of claiming untested results

If you compare EduTrack with Excel or another spreadsheet, use the **same three-student or larger fictional dataset**, the same person, the same computer and the same tasks. Time each workflow at least three times. Include the time needed to prepare formulas and identify the correct comparison group when applicable. A spreadsheet already set up with formulas is a different benchmark from a blank sheet; record that condition.

| Task | Spreadsheet trial 1 / 2 / 3 (seconds) | EduTrack trial 1 / 2 / 3 (seconds) | Notes |
| --- | --- | --- | --- |
| Find roll 102 and inspect marks | ___ / ___ / ___ | ___ / ___ / ___ | Same starting data |
| Correct a mark and verify the total | ___ / ___ / ___ | ___ / ___ / ___ | Include validation/undo as appropriate |
| Calculate named-subject group averages | ___ / ___ / ___ | ___ / ___ / ___ | Exclude the target where using peer comparisons |
| Evaluate a hypothetical 85% target | ___ / ___ / ___ | ___ / ___ / ___ | Do not modify saved marks |

Report median times and any errors after measuring. If the spreadsheet is faster for a task, say so and explain the project's learning goal instead of manipulating results.

## 6. Questions each member should be ready for

**Why not just Excel?** EduTrack is a focused academic workflow and DSA demonstration with built-in validation, named groups, undo, a FIFO queue and transactional local storage. A spreadsheet can also solve many of these tasks; superiority has to be measured.

**Why SQLite instead of plain text?** Related tables, foreign-key checks and transactions reduce inconsistent updates between records and group assignments. It still needs privacy controls and backups.

**Why a vector rather than a map for every operation?** A sorted vector permits simple traversal and binary search, but insertion/deletion shifts elements. This makes the performance trade-off clear in a DSA viva.

**Why are tied ranks equal?** Competition ranking compares exact mark ratios, so equally scoring students receive the same rank.

**Can the system predict marks?** No. The simulator and goal planner calculate mathematical alternatives, not real academic outcomes.

**What happens when two editors change a record?** The database commit checks for a stale loaded snapshot and rejects conflicting changes; the desktop is still intended for one editing instance at a time.

**Does undo survive restart?** No. Permanent records are in SQLite; the request queue and reverse-action history live in memory for the current session.

**What is the biggest limitation?** There is no user login/authorization, encryption by default, or multi-user deployment. Native Windows acceptance tests are required for the final build.
