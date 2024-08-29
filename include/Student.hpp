#pragma once

#include "Human.hpp"

class Student : public Human
{
private:
    /* data */
    std::string school_;
public:
    explicit Student(int age, const std::string name = "", const std::string school = "") : Human(age, name), school_(school) {};
    virtual ~Student() override;
    virtual char* print() const override;
};