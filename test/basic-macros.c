#define id(x) x
#define x id(123)
#define y (id(x) + 23)
#define z

#ifdef z
int main(void) { return x + id(y); }
#endif
