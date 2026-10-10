#ifndef STUDENT_H
#define STUDENT_H

#include <string>
#include <vector>

class Student {
public:
    Student(int rollNumber, std::string name, std::string branch,
            int semester, std::vector<int> marks);

    int rollNumber() const;
    const std::string& name() const;
    const std::string& branch() const;
    int semester() const;
    const std::vector<int>& marks() const;

private:
    int rollNumber_;
    std::string name_;
    std::string branch_;
    int semester_;
    std::vector<int> marks_;
};

#endif
