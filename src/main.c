/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lib/pp.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Expected a file as input");
        return 1;
    }
    String *path = from_cstr(argv[1]);

    Lexes *lexes = create_lexes(32);
    Preprocessor *pp = create_pp();

    Lex lex = include_file(pp, path);
    push_elem_vec(&lexes, &lex);

    do {
        lex = pp_lex_next(pp);
        push_elem_vec(&lexes, &lex);
    } while (lex.type != LEX_Eof);

    print_lexes(lexes);
    print_pp(pp);

    free(lexes);
    delete_pp(pp);

    return 0;
}
