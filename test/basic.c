// Tests a few basic patterns to catch obvious issues
#define hello 1

/*
 * A useless program
 */
int main(void) {
    int a = 0b11ull, b = 0x11lu, c = 0777ll, x = 556u;
    char z = 'z';
    z = L'z';
    z = U'z';
    char *y = "abcdef\n";
    y = u8"abcdef\n";
    unsigned int l[8] = U"abcdef\n";
    return 0;
}
