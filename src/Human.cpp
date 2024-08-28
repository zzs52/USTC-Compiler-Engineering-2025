#include "Human.hpp"

Human::~Human() {
    printf("Human destructor called\n");
}

void Human::print() const {
    printf("My name is %s and I am %d years old\n", name_.c_str(), age_);
}