#include "lib/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Expected a file as input");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    struct stat st_buf;

    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size);

    fread(buf, 1, st_buf.st_size, file);

    LexerOutput lexer = lex(buf, st_buf.st_size);

    print_lexes(lexer.lexes);

    print_ids(buf, lexer.ids);

    free(buf);
    fclose(file);
    return 0;
}
