#include "basic-macros.h"
#define help(x) x
#define id(x) help(x)
#define x 1
int main(void) { return help(x); }
