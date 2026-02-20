#include "pp.h"
#include "got.h"
#include "lexer.h"
#include "vec.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

Lex lex_next_top(Preprocessor *pp, Ids **id_table) {
    if (pp->incl_stack->length) {
        size_t *idx = at_elem_vec(pp->incl_stack, pp->incl_stack->length - 1);
        IncludeResource *resc = at_elem_vec(pp->incl_table, *idx);
        if (resc->type == IncludeFile) {
            return lex_next(&resc->stream, id_table);
        } else {
            return *(Lex *)at_elem_vec(resc->lexes, resc->idx++);
        }
    }
    return (Lex){.type = LEX_Eof};
}

Lex pp_lex_next(Preprocessor *pp, Ids **id_table) {
    return lex_next_top(pp, id_table);
}

Lex include_file(Preprocessor *pp, String *path) {
    FILE *file = fopen(path->s, "r");
    if (!file) {
        puts("Such a file could not be found");
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
    }

    struct stat st_buf;
    fstat(fileno(file), &st_buf);

    char *buf = malloc(st_buf.st_size + 1);

    size_t bytes = fread(buf, 1, st_buf.st_size, file);

    fclose(file);

    if (bytes == 0) {
        free(buf);
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
    }

    if (buf[bytes - 1] != '\\') {
        buf[bytes] = '\n';
    } else {
        puts("Last character cannot be '\\'");
        free(buf);
        return (Lex){.type = LEX_Invalid, .invalid = ExpectedValidIncludeFile};
    }

    push_elem_vec(&pp->incl_table, &(IncludeResource){
                                       .type = IncludeFile,
                                       .path = path,
                                       .stream = {.start = buf,
                                                  .len = bytes,
                                                  .idx = 0,
                                                  .row = 1,
                                                  .col = 0},
                                   });

    size_t id = pp->incl_table->length - 1;
    push_elem_vec(&pp->incl_stack, &id);

    return (Lex){0};
}

Preprocessor *create_pp() {
    Preprocessor *pp = malloc(sizeof(*pp));
    pp->incl_table = create_vec(4, sizeof(IncludeResource));
    pp->incl_stack = create_vec(4, sizeof(size_t));
    pp->macro_table = create_dht(8, sizeof(size_t), sizeof(DefineMacro));
    pp->macro_if_depth = 0;
    return pp;
}

void delete_pp(Preprocessor *pp) {
    for (size_t i = 0; i < pp->incl_table->length; i++) {
        IncludeResource *resc = at_elem_vec(pp->incl_table, i);
        if (resc->type == IncludeFile) {
            delete_str(resc->path);
            free(resc->stream.start);
        } else {
            delete_vec(resc->lexes);
        }
    }
    delete_vec(pp->incl_table);
    delete_vec(pp->incl_stack);
    delete_dht(pp->macro_table);
    free(pp);
}
