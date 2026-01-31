#include "basic-macros.h"
// #define id(x) x
#include "basic-macros.h"
#define x 100 + 50
#include "basic-macros.h"
// #define y (id(x) + 10)

#ifndef z
int friend() { return 0; }
int main(void) { return x + 20; }
#endif
