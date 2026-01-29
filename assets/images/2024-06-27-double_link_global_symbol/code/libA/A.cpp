
#include "A.h"

#include "Common.h"

void printInLibA() {
    printf("%s:%d kProblemSymbol:%d\n", __func__, __LINE__, Common::kProblemSymbol.value());
}