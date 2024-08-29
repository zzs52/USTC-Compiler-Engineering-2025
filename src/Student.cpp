#include "Student.hpp"
#include "cstring"

Student::~Student() {
    printf("Student destructor called\n");
}

char* Student::print() const {
    char *res = Human::print();
    sprintf(res + strlen(res), "I'm from %s\n", school_.c_str());
    return res;
}