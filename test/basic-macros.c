#define id(z) z
#define x id(100)
#define y (id(x) + 10)

#ifndef z
int main(void) { return x + id(y); }
#endif
