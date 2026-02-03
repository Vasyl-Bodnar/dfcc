#include "basic-macros.h"
#define id(z) z
#include "basic-macros.h"
#define x id(100)
#include "basic-macros.h"
#define y (id(x) + 10)

#ifndef z
int friend() { return 0; }
int main(void) { return x + id(y); }
#endif
