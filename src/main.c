#include "lib/lexer.h"
#include "lib/vec.h"
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

    char *buf = malloc(st_buf.st_size + 1);

    size_t bytes = fread(buf, 1, st_buf.st_size, file);

    if (bytes == 0) {
        goto Cleanup_buf_file;
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        goto Cleanup_buf_file;
    }

    Lexes *lexes = create_vec(16, sizeof(Lex));
    Ids *ids = create_vec(8, sizeof(Span));

    Lex lex;
    size_t idx = 0;
    do {
        lex = lex_next(buf, bytes, &idx, &ids);
        push_elem_vec(&lexes, &lex);
    } while (lex.type != Eof);

    print_lexes(lexes);
    print_ids(buf, ids);

    delete_vec(lexes);
    delete_vec(ids);

Cleanup_buf_file:
    free(buf);
    fclose(file);
    return 0;
}
