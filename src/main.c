/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lib/preprocessor.h"
#include "lib/vec.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Expected a file as input");
        return 1;
    }

    // TODO: Consider also supporting mmap if available
    FILE *file = fopen(argv[1], "r");
    struct stat st_buf;

    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size + 1);

    size_t bytes = fread(buf, 1, st_buf.st_size, file);

    if (bytes == 0) {
        fclose(file);
        free(buf);
        return 1;
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        fclose(file);
        free(buf);
        return 1;
    }

    IncludeStack *incl_stack = create_include_stack(4);
    Lexes *lexes = create_lexes(16);
    Ids *ids = create_ids(8);
    Macros *macros = create_macros(8);

    String *path = from_cstr(argv[1]);
    push_elem_vec(
        &incl_stack,
        &(IncludeFile){
            .file = file, .path = path, .input = buf, .len = bytes, .idx = 0});

    Lex lex;
    size_t depth = 0;
    do {
        lex = preprocessed_lex_next(&incl_stack, &depth, &ids, &macros);
        push_elem_vec(&lexes, &lex);
    } while (lex.type != LEX_Eof);

    print_lexes(lexes);
    print_ids(buf, ids);
    print_macros(buf, macros);
    print_include_stack(incl_stack);

    free(lexes);
    free(ids);
    free_macros(macros);
    free_include_stack(incl_stack);

    return 0;
}
