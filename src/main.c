/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lib/parser.h"
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Expected a file as input");
        return 1;
    }
    String *path = from_cstr(argv[1]);
    Parser *parser = create_parser(path);
    Ast ast = parse(parser);

    print_ast(ast, 0);
    print_parser(parser);

    // delete_ast
    delete_parser(parser);
    return 0;
}
