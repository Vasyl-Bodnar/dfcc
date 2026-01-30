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

    fclose(file);

    if (bytes == 0) {
        free(buf);
        return 1;
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        free(buf);
        return 1;
    }

    IncludeStack *incl_stack = create_include_stack(16);
    Lexes *lexes = create_lexes(32);
    Ids *ids = create_ids(16);
    Includes *includes = create_includes(4);
    Macros *macros = create_macros(8);

    String *path = from_cstr(argv[1]);
    push_elem_vec(
        &includes,
        &(IncludeFile){.path = path, .input = buf, .len = bytes, .idx = 0});

    size_t incl_id = 0;
    push_elem_vec(&incl_stack, &incl_id);

    Lex lex;
    size_t depth = 0;
    while (incl_stack->length) {
        do {
            lex = preprocessed_lex_next(&incl_stack, &depth, &ids, &includes,
                                        &macros);
            push_elem_vec(&lexes, &lex);
        } while (lex.type != LEX_Eof);
        IncludeFile *file = get_top_include(incl_stack, includes);
        file->idx = 0;
        pop_elem_vec(incl_stack);
    }

    print_lexes(lexes);
    print_ids(ids);
    print_macros(macros);
    print_includes(includes);

    free(lexes);
    free(ids);
    free_macros(macros);
    free_includes(includes);
    free_include_stack(incl_stack);

    return 0;
}
