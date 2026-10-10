#include "Student.h"

#include <utility>

Student::Student(int rollNumber, std::string name, std::string branch,
                 int semester, std::vector<int> marks)
    : rollNumber_(rollNumber), name_(std::move(name)),
      branch_(std::move(branch)), semester_(semester), marks_(std::move(marks)) {}

int Student::rollNumber() const { return rollNumber_; }

const std::string& Student::name() const { return name_; }

const std::string& Student::branch() const { return branch_; }

int Student::semester() const { return semester_; }

const std::vector<int>& Student::marks() const { return marks_; }
