#define id(x) x
#define x id(100)
#define y (id(x) + 10)
#define z

#ifndef z
int main(void) { return x + id(y); }
#endif
