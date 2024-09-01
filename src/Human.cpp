#include "Human.hpp"

Human::~Human() {
    printf("Human destructor called\n");
}

char* Human::print() const {
    char* res = new char[100];
    sprintf(res, "My name is %s and I am %d years old\n", name_.c_str(), age_);
    return res;
}