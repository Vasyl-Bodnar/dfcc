#include "basic-macros.h"
#define id(x) x
#define x 1
#define y 2
int main(void) { return id(x) + id(y) + id(50); }
