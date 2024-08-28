#include <stdio.h>
#include "Human.hpp"
#include "Student.hpp"
#include <vector>

int main(int, char**){
    printf("Hello, from stl_debug!\n");
    std::vector<Human*> vec;
    Human human(25, "John Doe");
    Student student(20, "Jane Doe", "MIT");
    vec.push_back(&human);
    vec.push_back(&student);
    for (const auto& h : vec) {
        h->print();
    }
    static_cast<Student*>(vec.back())->print();
}
