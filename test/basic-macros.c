<<<<<<< HEAD
#define id(z) z
#define x id(100)
#define y (id(x) + 10)

#ifndef z
int main(void) { return x + id(y); }
=======
#include "basic-macros.h"
// #define id(x) x
#include "basic-macros.h"
#define x 100 + 50
#include "basic-macros.h"
// #define y (id(x) + 10)

#ifndef z
int friend() { return 0; }
int main(void) { return x + 20; }
>>>>>>> parent of 2a8cb0f (Made function macros work, some refactoring)
#endif
