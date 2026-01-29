
#include "B.h"

#include "Common.h"

void printInLibB() {
    printf("%s:%d kProblemSymbol:%d\n", __func__, __LINE__, Common::kProblemSymbol.value());
}