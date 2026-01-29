#pragma once

#include <stdio.h>

class DemoClass {
public:
    DemoClass(int value);
    ~DemoClass();

    int value() const;

private:
    int* mValue;
};