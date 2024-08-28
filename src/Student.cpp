#include "Student.hpp"

Student::~Student() {
    printf("Student destructor called\n");
}

void Student::print() const {
    Human::print();
    printf("I'm from %s\n", school_.c_str());
}