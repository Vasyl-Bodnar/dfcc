/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lib/preprocessor.h"
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

    Lexes *lexes = create_lexes(16);
    Ids *ids = create_ids(8);
    Macros *macros = create_macros(8);

    Lex lex;
    size_t idx = 0;
    size_t depth = 0;
    do {
        lex = preprocessed_lex_next(buf, bytes, &idx, &depth, &ids, &macros);
        push_elem_vec(&lexes, &lex);
    } while (lex.type != Eof);

    print_lexes(lexes);
    print_ids(buf, ids);

    free(lexes);
    free(ids);
    free(macros);

Cleanup_buf_file:
    free(buf);
    fclose(file);
    return 0;
}
