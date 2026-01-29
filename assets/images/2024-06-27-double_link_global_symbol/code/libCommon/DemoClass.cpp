
#include "DemoClass.h"

DemoClass::DemoClass(int value) {
    printf("%s:%d this:%p\n", __func__, __LINE__, this);
    mValue = new int(value);
}

int DemoClass::value() const {
    return *mValue;
}

DemoClass::~DemoClass() {
    printf("%s:%d this:%p\n", __func__, __LINE__, this);
    delete mValue;
}
