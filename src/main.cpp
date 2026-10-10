#include "RecordAlgorithms.h"
#include "StudentManagement.h"

#include <cctype>
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

        bool hasLetters = false;
        for (unsigned char c : value) {
            if (!std::isspace(c)) {
                hasLetters = true;
                break;
            }
        }
        if (hasLetters && value.size() <= maxLength) return value;
        std::cout << "Enter non-empty text (maximum " << maxLength << " characters).\n";
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

void showSummary(const Student& student) {
    std::cout << "Roll " << student.rollNumber() << " | " << student.name()
              << " | Total: " << RecordAlgorithms::totalMarks(student)
              << " | Percentage: " << std::fixed << std::setprecision(2)
              << RecordAlgorithms::percentage(student) << "%\n";
}

void showOrdered(const RecordAlgorithms::StudentView& students) {
    if (students.empty()) {
        std::cout << "No student records yet.\n";
        return;
    }
    for (const Student* student : students) showSummary(*student);
}

void showRankList(const std::vector<Student>& students) {
    const auto ranked = RecordAlgorithms::rankList(students);
    if (ranked.empty()) {
        std::cout << "No student records yet.\n";
        return;
    }
    std::cout << "Rank list (percentage, then total marks, then roll number):\n";
    for (const auto& entry : ranked) {
        std::cout << "Rank " << entry.rank << ": ";
        showSummary(*entry.student);
    }
}

void showMenu() {
    std::cout << "\n========== EduTrack (Part 2) ==========\n"
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
              << "0. Exit\n";
}
}  // namespace

int main() {
    StudentManagement records;
    std::cout << "Records are stored in memory for this version; they are not saved yet.\n";

    while (true) {
        showMenu();
        auto choice = readNumber("Choice: ", 0, 10);
        if (!choice || *choice == 0) break;

        if (*choice == 2) {
            const auto& students = records.allStudents();
            if (students.empty()) std::cout << "No student records yet.\n";
            for (const Student& student : students) showStudent(student);
            continue;
        }
        if (*choice == 7) {
            std::cout << "Students sorted by percentage:\n";
            showOrdered(RecordAlgorithms::sortByPercentage(records.allStudents()));
            continue;
        }
        if (*choice == 8) {
            std::cout << "Students sorted by total marks:\n";
            showOrdered(RecordAlgorithms::sortByTotalMarks(records.allStudents()));
            continue;
        }
        if (*choice == 9) {
            showRankList(records.allStudents());
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
            std::cout << (records.deleteStudent(*roll) ? "Student deleted.\n"
                                                       : "Student not found.\n");
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
            const bool saved = (*choice == 1) ? records.addStudent(*student)
                                               : records.updateStudent(*roll, *student);
            std::cout << (saved ? (*choice == 1 ? "Student added.\n" : "Student updated.\n")
                                 : "Could not save record.\n");
        }
    }

    std::cout << "Goodbye. Records from this session were not saved.\n";
}
