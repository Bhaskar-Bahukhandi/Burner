#include "AcademicProcessing.h"
#include "RecordAlgorithms.h"

#include <algorithm>

namespace AcademicProcessing {

std::string grade(double percentage) {
    if (percentage >= 90.0) return "A";
    if (percentage >= 80.0) return "B";
    if (percentage >= 70.0) return "C";
    if (percentage >= 60.0) return "D";
    return "F";
}

std::string category(double percentage) {
    if (percentage >= 85.0) return "Excellent";
    if (percentage >= 70.0) return "Good";
    if (percentage >= 50.0) return "Average";
    return "Needs Improvement";
}

std::optional<double> classAverage(const std::vector<Student>& students) {
    if (students.empty()) return std::nullopt;

    double sum = 0.0;
    for (const Student& student : students) {
        sum += RecordAlgorithms::percentage(student);
    }
    return sum / static_cast<double>(students.size());
}

std::vector<SubjectResult> subjectResults(const std::vector<Student>& students) {
    std::vector<SubjectResult> result;
    for (const Student& student : students) {
        const auto& marks = student.marks();
        for (std::size_t i = 0; i < marks.size(); ++i) {
            if (i == result.size()) result.push_back({i + 1, 0, 0.0, 100, 0});
            SubjectResult& subject = result[i];
            subject.average += marks[i]; // Sum first, divide once after all records.
            ++subject.students;
            subject.lowest = std::min(subject.lowest, marks[i]);
            subject.highest = std::max(subject.highest, marks[i]);
        }
    }
    for (SubjectResult& subject : result) {
        subject.average /= static_cast<double>(subject.students);
    }
    return result;
}

const Student* topStudent(const std::vector<Student>& students) {
    auto ranked = RecordAlgorithms::sortByPercentage(students);
    return ranked.empty() ? nullptr : ranked.front();
}

}  // namespace AcademicProcessing
