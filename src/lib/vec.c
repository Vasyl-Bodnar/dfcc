/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vec.h"
#include <string.h>

// Generic Vector
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

void *peek_elem_vec(Vector *v) {
    return (void *)(v->v + v->value_size * (v->length - 1));
}

void pop_elem_vec(Vector *v) { v->length -= 1; }

void *at_elem_vec(Vector *v, size_t idx) {
    return (void *)(v->v + v->value_size * idx);
}

void get_elem_vec(Vector *v, size_t idx, void *value) {
    memcpy(value, v->v + idx, v->value_size);
}

void delete_vec(Vector *v) { free(v); }

// String
String *create_str(size_t len) {
    String *s = malloc(sizeof(Vector) + len);
    if (!s) {
        return s;
    }
    s->length = 0;
    s->capacity = len;
    memset(s->s, 0, len);
    return s;
}

String *from_cstr(char *cs) {
    size_t len = strlen(cs) + 1; // Include null
    String *s = create_str(len);
    if (!push_slice_str(&s, cs, len)) {
        return 0;
    }
    return s;
}

uint32_t push_elem_str(String **s, char value) {
    if ((*s)->length >= (*s)->capacity) {
        size_t new_capacity =
            ((*s)->capacity < 4) ? 4 : (((*s)->capacity * 3) / 2);
        String *new_s = realloc(*s, sizeof(Vector) + new_capacity);
        if (!new_s) {
            return 0;
        } else {
            *s = new_s;
            (*s)->capacity = new_capacity;
        }
    }

    (*s)->s[(*s)->length] = value;
    (*s)->length += 1;

    return 1;
}

uint32_t push_slice_str(String **s, char *start, size_t len) {
    if (!len) {
        return 1;
    }

    if ((*s)->length + len > (*s)->capacity) {
        size_t new_len = (*s)->length + len;
        size_t new_capacity = (new_len < 4) ? 4 : ((new_len * 3) / 2);
        String *new_s = realloc(*s, sizeof(Vector) + new_capacity);
        if (!new_s) {
            return 0;
        } else {
            *s = new_s;
            (*s)->capacity = new_capacity;
        }
    }

    memcpy((*s)->s + (*s)->length, start, len);
    (*s)->length += len;

    return 1;
}

char peek_elem_str(String *s) { return s->s[s->length - 1]; }

void pop_elem_str(String *s) { s->length -= 1; }

void delete_str(String *s) { free(s); }
