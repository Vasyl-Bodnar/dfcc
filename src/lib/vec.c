/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vec.h"

Vector *create_vec(size_t len, size_t value_size) {
    Vector *v = malloc(sizeof(Vector) + len * value_size);
    if (!v) {
        return v;
    }
    v->value_size = value_size;
    v->length = 0;
    v->capacity = len;
    memset(v->v, 0, len * value_size);
    return v;
}

uint32_t push_elem_vec(Vector **v, const void *value) {
    if ((*v)->length >= (*v)->capacity) {
        size_t new_capacity =
            ((*v)->capacity < 4) ? 4 : (((*v)->capacity * 3) / 2);
        Vector *new_v =
            realloc(*v, sizeof(Vector) + new_capacity * (*v)->value_size);
        if (!new_v) {
            return 0;
        } else {
            *v = new_v;
            (*v)->capacity = new_capacity;
        }
    }

    memcpy((*v)->v + (*v)->length * (*v)->value_size, value, (*v)->value_size);
    (*v)->length += 1;

    return 1;
}

void *at_vec(Vector *v, size_t idx) {
    if (idx > v->length) {
        return 0;
    }
    return (void *)(v->v + idx);
}

uint32_t get_elem_vec(Vector *v, size_t idx, void *value) {
    if (idx > v->length) {
        return 0;
    }

    memcpy(value, v->v + idx, v->value_size);

    return 1;
}

void get_nocheck_elem_vec(Vector *v, size_t idx, void *value) {
    memcpy(value, v->v + idx, v->value_size);
}

void delete_vec(Vector *v) { free(v); }
