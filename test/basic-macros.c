#include "basic-macros.h"
#define id(x) x
#include "basic-macros.h"
#define x id(100)
#include "basic-macros.h"
#define y (id(x) + 10)
#define z

#ifndef z
int friend() { return 0; }
int main(void) { return x + id(y); }
#endif
