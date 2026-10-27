#include "AcademicProcessing.h"
#include "AcademicGroupStorage.h"
#include "GroupConsole.h"
#include "ChangeHistory.h"
#include "RecordStorage.h"
#include "RecordAlgorithms.h"
#include "RequestQueue.h"
#include "StudentManagement.h"

#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
std::optional<int> readNumber(const std::string& prompt, int min, int max) {
    std::string line;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, line)) return std::nullopt;

        std::istringstream input(line);
        int number;
        char extra;
        if ((input >> number) && !(input >> extra) && number >= min && number <= max) {
            return number;
        }
        std::cout << "Enter a whole number from " << min << " to " << max << ".\n";
    }
}

std::optional<std::string> readText(const std::string& prompt, std::size_t maxLength) {
    std::string value;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, value)) return std::nullopt;

        bool hasText = false;
        bool hasControl = false;
        for (unsigned char c : value) {
            if (std::iscntrl(c)) hasControl = true;
            if (!std::isspace(c)) hasText = true;
        }
        if (hasText && !hasControl && value.size() <= maxLength) return value;
        std::cout << "Enter non-empty text without control characters (maximum "
                  << maxLength << " characters).\n";
    }
}

std::optional<Student> enterStudent(int rollNumber) {
    auto name = readText("Name: ", 80);
    if (!name) return std::nullopt;
    auto branch = readText("Branch: ", 60);
    if (!branch) return std::nullopt;
    auto semester = readNumber("Semester (1-12): ", 1, 12);
    if (!semester) return std::nullopt;
    auto count = readNumber("Number of subjects (1-20): ", 1, 20);
    if (!count) return std::nullopt;

    std::vector<int> marks;
    for (int i = 1; i <= *count; ++i) {
        auto mark = readNumber("Subject " + std::to_string(i) + " marks (0-100): ", 0, 100);
        if (!mark) return std::nullopt;
        marks.push_back(*mark);
    }
    return Student(rollNumber, std::move(*name), std::move(*branch), *semester,
                   std::move(marks));
}

void showStudent(const Student& student) {
    std::cout << "\nRoll number: " << student.rollNumber()
              << "\nName: " << student.name()
              << "\nBranch: " << student.branch()
              << "\nSemester: " << student.semester()
              << "\nMarks: ";
    for (std::size_t i = 0; i < student.marks().size(); ++i) {
        if (i != 0) std::cout << ", ";
        std::cout << student.marks()[i];
    }
    std::cout << "\n";
}

void showSummary(const Student& student, AcademicProcessing::GradeScale scale) {
    std::cout << "Roll " << student.rollNumber() << " | " << student.name()
              << " | Total: " << RecordAlgorithms::totalMarks(student)
              << " | Percentage: " << std::fixed << std::setprecision(2)
              << RecordAlgorithms::percentage(student) << "%"
              << " | Grade: " << AcademicProcessing::grade(RecordAlgorithms::percentage(student), scale)
              << " | " << AcademicProcessing::category(RecordAlgorithms::percentage(student), scale)
              << "\n";
}

void showOrdered(const RecordAlgorithms::StudentView& students, AcademicProcessing::GradeScale scale) {
    if (students.empty()) {
        std::cout << "No student records yet.\n";
        return;
    }
    for (const Student* student : students) showSummary(*student, scale);
}

void showRankList(const std::vector<Student>& students, AcademicProcessing::GradeScale scale) {
    const auto ranked = RecordAlgorithms::rankList(students);
    if (ranked.empty()) {
        std::cout << "No student records yet.\n";
        return;
    }
    std::cout << "Rank list (percentage, then total marks, then roll number):\n";
    for (const auto& entry : ranked) {
        std::cout << "Rank " << entry.rank << ": ";
        showSummary(*entry.student, scale);
    }
}

const char* requestName(RequestType type) {
    switch (type) {
        case RequestType::Add: return "Add";
        case RequestType::Update: return "Update";
        case RequestType::Delete: return "Delete";
    }
    return "Unknown";
}

void displayQueue(const RequestQueue& queue) {
    const auto requests = queue.pending();
    if (requests.empty()) {
        std::cout << "No pending requests.\n";
        return;
    }
    std::cout << "Pending requests (first to last):\n";
    for (std::size_t i = 0; i < requests.size(); ++i) {
        std::cout << (i + 1) << ". " << requestName(requests[i].type)
                  << " | Roll " << requests[i].rollNumber << "\n";
    }
}

void reportResult(const std::optional<ProcessResult>& result) {
    if (!result) {
        std::cout << "No pending requests.\n";
        return;
    }
    std::cout << requestName(result->request.type)
              << " request for roll " << result->request.rollNumber
              << (result->applied ? " completed.\n"
                                  : " rejected (record missing, duplicate, or invalid). "
                                    "The request was removed; records were not changed.\n");
}

bool queueRequest(RequestQueue& queue) {
    std::cout << "Request types: 1. Add  2. Update  3. Delete\n";
    auto type = readNumber("Request type: ", 1, 3);
    if (!type) return false;
    auto roll = readNumber("Roll number: ", 1, 999999999);
    if (!roll) return false;

    if (*type == 3) {
        queue.enqueueDelete(*roll);
    } else {
        auto student = enterStudent(*roll);
        if (!student) return false;
        if (*type == 1) queue.enqueueAdd(*student);
        else queue.enqueueUpdate(*student);
    }
    std::cout << "Request queued. Pending: " << queue.size() << "\n";
    return true;
}

void showStudentReport(const StudentManagement& records, int roll, AcademicProcessing::GradeScale scale) {
    const Student* student = records.findStudent(roll);
    if (!student) {
        std::cout << "Student not found.\n";
        return;
    }
    showStudent(*student);
    showSummary(*student, scale);
    const auto average = AcademicProcessing::classAverage(records.allStudents());
    if (average) {
        const double difference = RecordAlgorithms::percentage(*student) - *average;
        std::cout << "Class average: " << std::fixed << std::setprecision(2)
                  << *average << "% | Difference: " << std::showpos
                  << difference << std::noshowpos << " percentage points\n";
    }
}

void showClassReport(const StudentManagement& records, AcademicProcessing::GradeScale scale) {
    const auto& students = records.allStudents();
    const auto average = AcademicProcessing::classAverage(students);
    if (!average) {
        std::cout << "No student records yet.\n";
        return;
    }
    std::cout << "Class size: " << students.size() << "\n"
              << "Average percentage: " << std::fixed << std::setprecision(2)
              << *average << "%\n";
    const Student* topper = AcademicProcessing::topStudent(students);
    if (topper) {
        std::cout << "Top performer: ";
        showSummary(*topper, scale);
    }
    std::size_t excellent = 0, good = 0, averageCount = 0, needsImprovement = 0;
    for (const Student& student : students) {
        const std::string band = AcademicProcessing::category(
            RecordAlgorithms::percentage(student), scale);
        if (band == "Excellent") ++excellent;
        else if (band == "Good") ++good;
        else if (band == "Average") ++averageCount;
        else ++needsImprovement;
    }
    std::cout << "Performance bands: Excellent " << excellent
              << ", Good " << good << ", Average " << averageCount
              << ", Needs Improvement " << needsImprovement << "\n";
    bool mixedSubjectCounts = false;
    for (const Student& student : students) {
        if (student.marks().size() != students.front().marks().size()) {
            mixedSubjectCounts = true;
            break;
        }
    }
    if (mixedSubjectCounts) {
        std::cout << "Note: Students have different numbers of subjects. "
                     "Each subject average includes only records with that position; "
                     "enter subjects in the same order for valid comparisons.\n";
    }
    for (const SubjectResult& subject : AcademicProcessing::subjectResults(students)) {
        std::cout << "Subject " << subject.number << " (" << subject.students
                  << (subject.students == 1 ? " student" : " students")
                  << "): average " << std::fixed << std::setprecision(2)
                  << subject.average << ", lowest " << subject.lowest
                  << ", highest " << subject.highest << "\n";
    }
}

bool confirmation(const std::string& prompt) {
    std::cout << prompt << " Type YES to confirm: ";
    std::string answer;
    return std::getline(std::cin, answer) && answer == "YES";
}

// Do not change records in a way that makes saved group assignments invalid.
// Work on copies first; unsuccessful or blocked operations leave undo/queue intact.
template <typename Operation>
bool guardedChange(StudentManagement& records, ChangeHistory& history,
                   const AcademicGroups& groups, Operation operation) {
    StudentManagement trialRecords = records;
    ChangeHistory trialHistory = history;
    if (!operation(trialRecords, trialHistory)) return false;
    if (!groups.consistentWith(trialRecords)) {
        std::cout << "Blocked: this change would invalidate an academic group assignment. "
                     "Unassign the student first.\n";
        return false;
    }
    records = std::move(trialRecords);
    history = std::move(trialHistory);
    return true;
}

// Process on copies. When a queued request conflicts with group metadata,
// retain it for later instead of consuming it or changing records.
bool processSafely(StudentManagement& records, ChangeHistory& history,
                   RequestQueue& queue, const AcademicGroups& groups,
                   bool& dirty) {
    if (queue.size() == 0) { std::cout << "No pending requests.\n"; return true; }
    StudentManagement trialRecords = records;
    ChangeHistory trialHistory = history;
    RequestQueue trialQueue = queue;
    const auto result = trialQueue.processNext(trialRecords, trialHistory);
    if (result && result->applied && !groups.consistentWith(trialRecords)) {
        std::cout << "Request blocked by an academic group assignment. "
                     "It is still queued; unassign the student before processing.\n";
        return false;
    }
    if (result && result->applied) {
        records = std::move(trialRecords);
        history = std::move(trialHistory);
        dirty = true;
    }
    queue = std::move(trialQueue);
    reportResult(result);
    return true;
}

bool checkedExists(const std::string& file, bool& present) {
    std::error_code error;
    present = std::filesystem::exists(file, error);
    if (!error) return true;
    std::cerr << "Cannot inspect " << file << ": " << error.message() << "\n";
    return false;
}

// Reload uses the same recovery-file rules as startup. Rejecting leftovers
// matters because a .bak/.tmp may mean the previous save was interrupted.
// Load both files into temporary objects; failures keep live state unchanged.
bool loadBoth(const std::string& studentsFile, const std::string& groupsFile,
              StudentManagement& records, AcademicGroups& groups,
              std::string& error) {
    namespace fs = std::filesystem;
    error.clear();
    for (const std::string& file : {studentsFile, groupsFile}) {
        for (const char* suffix : {".tmp", ".bak"}) {
            std::error_code ec;
            const std::string recovery = file + suffix;
            const bool exists = fs::exists(recovery, ec);
            if (ec) {
                error = "Cannot inspect recovery file " + recovery + ": " + ec.message();
                return false;
            }
            if (exists) {
                error = "Recovery file " + recovery +
                        " exists. Inspect it before reloading.";
                return false;
            }
        }
    }
    std::error_code ec;
    const bool studentsExist = fs::exists(studentsFile, ec);
    if (ec) { error = "Cannot inspect student data: " + ec.message(); return false; }
    StudentManagement proposedRecords;
    // A fresh project with no students.txt is a valid empty dataset, just
    // as it is during application startup.
    if (studentsExist && !RecordStorage::load(studentsFile, proposedRecords, error))
        return false;
    ec.clear();
    const bool groupsExist = fs::exists(groupsFile, ec);
    if (ec) { error = "Cannot inspect group data: " + ec.message(); return false; }
    AcademicGroups proposedGroups;
    if (groupsExist && !AcademicGroupStorage::load(groupsFile, proposedRecords,
                                                    proposedGroups, error)) return false;
    if (!proposedGroups.consistentWith(proposedRecords)) {
        error = "Saved academic-group assignments do not match student records.";
        return false;
    }
    records = std::move(proposedRecords);
    groups = std::move(proposedGroups);
    return true;
}

void showMenu() {
    std::cout << "\n========== EduTrack ==========\n"
              << "1. Add student\n"
              << "2. Display all students\n"
              << "3. Linear search by roll number\n"
              << "4. Update student\n"
              << "5. Delete student\n"
              << "6. Binary search by roll number\n"
              << "7. Sort students by percentage\n"
              << "8. Sort students by total marks\n"
              << "9. Show rank list\n"
              << "10. Find rank by roll number\n"
              << "11. Queue a record request\n"
              << "12. Process next request\n"
              << "13. Show pending requests\n"
              << "14. Undo last successful change\n"
              << "15. Process all pending requests\n"
              << "16. Student academic report\n"
              << "17. Class and subject report\n"
              << "18. Save student records\n"
              << "19. Reload saved records\n"
              << "20. Change grading scheme / show criteria\n"
              << "21. Create academic group\n"
              << "22. Assign student to group\n"
              << "23. Unassign student from group\n"
              << "24. List academic groups and subjects\n"
              << "25. Simulate mark and comparable group rank\n"
              << "26. View group-based academic insights\n"
              << "27. Plan a hypothetical percentage or grade goal\n"
              << "0. Save and exit\n";
}
}  // namespace

int main() {
    StudentManagement records;
    ChangeHistory history;
    RequestQueue queue;
    AcademicGroups groups;
    const std::string groupFile = "academic_groups.txt";
    const std::string filename = "students.txt";
    bool dirty = false;
    auto gradeScale = AcademicProcessing::GradeScale::GeuReference;
    namespace fs = std::filesystem;
    std::error_code fileError;
    const bool hasData = fs::exists(filename, fileError);
    if (fileError) {
        std::cerr << "Cannot inspect saved records: " << fileError.message() << "\n";
        return 1;
    }
    // Check each recovery file separately so an error from the first check
    // cannot be overwritten by the second filesystem call.
    for (const char* extension : {".bak", ".tmp"}) {
        fileError.clear();
        const std::string recoveryFile = filename + extension;
        const bool present = fs::exists(recoveryFile, fileError);
        if (fileError) {
            std::cerr << "Cannot inspect " << recoveryFile << ": "
                      << fileError.message() << "\n";
            return 1;
        }
        if (present) {
            std::cerr << "Recovery file " << recoveryFile
                      << " exists. Check the saved records before restarting.\n";
            return 1;
        }
    }
    if (hasData) {
        std::string error;
        if (!RecordStorage::load(filename, records, error)) {
            std::cerr << "Cannot load saved records: " << error
                      << ". The file was not changed.\n";
            return 1;
        }
        std::cout << "Loaded " << records.allStudents().size()
                  << " saved student records.\n";
    } else {
        std::cout << "No saved file found. Starting with empty records.\n";
    }
    // A partial metadata save must be inspected before opening the app.
    for (const char* extension : {".bak", ".tmp"}) {
        bool present = false;
        if (!checkedExists(groupFile + extension, present)) return 1;
        if (present) {
            std::cerr << "Recovery metadata found: " << groupFile << extension
                      << ". Inspect it before continuing.\n";
            return 1;
        }
    }
    bool groupsPresent = false;
    if (!checkedExists(groupFile, groupsPresent)) return 1;
    if (groupsPresent) {
        std::string error;
        if (!AcademicGroupStorage::load(groupFile, records, groups, error)) {
            std::cerr << "Cannot load academic groups: " << error
                      << ". Student records were not changed.\n";
            return 1;
        }
        std::cout << "Loaded " << groups.groups().size() << " academic groups.\n";
    }
    std::cout << "Grading: " << AcademicProcessing::scaleName(gradeScale) << "\n";
    std::cout << "Record changes are saved on exit or by option 18. "
                 "Undo history and queued requests are not saved.\n";

    while (true) {
        showMenu();
        auto choice = readNumber("Choice: ", 0, 27);
        if (!choice || *choice == 0) {
            if (queue.size() && choice &&
                !confirmation("Pending requests will be discarded without processing.")) {
                continue;
            }
            if (dirty) {
                std::string error;
                if (!RecordStorage::save(filename, records, error)) {
                    std::cerr << "Save failed: " << error << "\n";
                    if (!choice) return 1;  // Input ended; cannot offer recovery.
                    std::cerr << "Records remain in memory. Resolve the issue and retry.\n";
                    continue;
                }
                dirty = false;
                std::cout << "Saved student records.\n";
                if (!error.empty()) std::cout << "Note: " << error << "\n";
            }
            break;
        }
        if (*choice >= 21 && *choice <= 27) {
            // Metadata saves immediately. First flush pending marks to avoid
            // persisting assignments for students absent from students.txt.
            if ((*choice == 21 || *choice == 22 || *choice == 23) && dirty) {
                std::string error;
                if (!RecordStorage::save(filename, records, error)) {
                    std::cout << "Save students first: " << error << "\n";
                    continue;
                }
                dirty = false;
                if (!error.empty()) std::cout << "Note: " << error << "\n";
            }
            if (!GroupConsole::handle(*choice, records, groups, groupFile,
                                      gradeScale, std::cin, std::cout)) break;
            continue;
        }
        if (*choice == 20) {
            std::cout << "Grading options:\n"
                      << "1. GEU published distance-learning reference (illustrative)\n"
                      << "   >85 O, 75-85 A+, 65-<75 A, 55-<65 B+, "
                         "50-<55 B, 45-<50 C, 40-<45 P, below 40 F\n"
                      << "   Exact 85% is treated as A+ because the source table leaves 85 unspecified.\n"
                      << "2. Original EduTrack project scale\n"
                      << "   90+ A, 80+ B, 70+ C, 60+ D, below 60 F\n"
                      << "Bands (Excellent/Good/Average/Needs Improvement) "
                         "are separate project categories.\n";
            auto selected = readNumber("Scheme (1 or 2): ", 1, 2);
            if (!selected) break;
            gradeScale = *selected == 1 ? AcademicProcessing::GradeScale::GeuReference
                                        : AcademicProcessing::GradeScale::OriginalProject;
            std::cout << "Now using: " << AcademicProcessing::scaleName(gradeScale)
                      << " (student records unchanged).\n";
            continue;
        }
        if (*choice == 18) {
            std::string error;
            if (RecordStorage::save(filename, records, error)) {
                dirty = false;
                std::cout << "Saved " << records.allStudents().size()
                          << " student records.\n";
                if (!error.empty()) std::cout << "Note: " << error << "\n";
            } else std::cout << "Save failed: " << error << "\n";
            continue;
        }
        if (*choice == 19) {
            if ((dirty || queue.size() || history.undoCount()) &&
                !confirmation("Reloading will replace records and clear undo/queue history.")) {
                std::cout << "Reload cancelled.\n";
                continue;
            }
            std::string error;
            if (loadBoth(filename, groupFile, records, groups, error)) {
                history.clear();
                queue.clear();
                dirty = false;
                std::cout << "Records and group metadata reloaded. Undo history and queue cleared.\n";
            } else {
                std::cout << "Reload failed: " << error
                          << ". Current records, history and queue were kept.\n";
            }
            continue;
        }
        if (*choice == 17) {
            showClassReport(records, gradeScale);
            continue;
        }
        if (*choice == 16) {
            auto roll = readNumber("Roll number: ", 1, 999999999);
            if (!roll) break;
            showStudentReport(records, *roll, gradeScale);
            continue;
        }

        if (*choice == 2) {
            const auto& students = records.allStudents();
            if (students.empty()) std::cout << "No student records yet.\n";
            for (const Student& student : students) showStudent(student);
            continue;
        }
        if (*choice == 7) {
            std::cout << "Students sorted by percentage:\n";
            showOrdered(RecordAlgorithms::sortByPercentage(records.allStudents()), gradeScale);
            continue;
        }
        if (*choice == 8) {
            std::cout << "Students sorted by total marks:\n";
            showOrdered(RecordAlgorithms::sortByTotalMarks(records.allStudents()), gradeScale);
            continue;
        }
        if (*choice == 9) {
            showRankList(records.allStudents(), gradeScale);
            continue;
        }
        if (*choice == 11) {
            if (!queueRequest(queue)) break;
            continue;
        }
        if (*choice == 12) {
            processSafely(records, history, queue, groups, dirty);
            continue;
        }
        if (*choice == 13) {
            displayQueue(queue);
            continue;
        }
        if (*choice == 14) {
            if (guardedChange(records, history, groups,
                  [](StudentManagement& r, ChangeHistory& h) {return h.undo(r);})) {
                dirty = true;
                std::cout << "Last change undone.\n";
            } else std::cout << "Nothing undone (empty history or group conflict).\n";
            continue;
        }
        if (*choice == 15) {
            if (queue.size() == 0) {
                std::cout << "No pending requests.\n";
            } else {
                while (queue.size() != 0) {
                    if (!processSafely(records, history, queue, groups, dirty)) break;
                }
            }
            continue;
        }

        auto roll = readNumber("Roll number: ", 1, 999999999);
        if (!roll) break;

        if (*choice == 3 || *choice == 6) {
            const Student* student = (*choice == 3)
                ? RecordAlgorithms::linearSearch(records.allStudents(), *roll)
                : RecordAlgorithms::binarySearch(records.allStudents(), *roll);
            if (student) showStudent(*student);
            else std::cout << "Student not found.\n";
        } else if (*choice == 10) {
            const auto rank = RecordAlgorithms::rankOf(records.allStudents(), *roll);
            if (rank) std::cout << "Rank: " << *rank << "\n";
            else std::cout << "Student not found.\n";
        } else if (*choice == 5) {
            if (guardedChange(records, history, groups,
                    [roll](StudentManagement& r, ChangeHistory& h) {
                        return h.deleteStudent(r, *roll);
                    })) {
                dirty = true;
                std::cout << "Student deleted.\n";
            } else std::cout << "Student not found.\n";
        } else if (*choice == 1 || *choice == 4) {
            const bool exists = records.findStudent(*roll) != nullptr;
            if (*choice == 1 && exists) {
                std::cout << "That roll number already exists.\n";
                continue;
            }
            if (*choice == 4 && !exists) {
                std::cout << "Student not found.\n";
                continue;
            }

            auto student = enterStudent(*roll);
            if (!student) break;
            const bool saved = guardedChange(records, history, groups,
                [choice, roll, &student](StudentManagement& r, ChangeHistory& h) {
                    return *choice == 1 ? h.addStudent(r, *student)
                                        : h.updateStudent(r, *roll, *student);
                });
            if (saved) dirty = true;
            std::cout << (saved ? (*choice == 1 ? "Student added.\n" : "Student updated.\n")
                                 : "Could not save record.\n");
        }
    }

    // An EOF during a form also exits the loop. Save those edits as well.
    if (dirty) {
        std::string error;
        if (!RecordStorage::save(filename, records, error)) {
            std::cerr << "Save failed on exit: " << error
                      << ". Unsaved records remain only in this session.\n";
            return 1;
        }
        std::cout << "Saved student records.\n";
        if (!error.empty()) std::cout << "Note: " << error << "\n";
    }
    if (queue.size() != 0) {
        std::cout << "Warning: " << queue.size()
                  << " unprocessed queued requests were not saved.\n";
    }
    std::cout << "Goodbye. Undo history and queued requests were not saved.\n";
}
