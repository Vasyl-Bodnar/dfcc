/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Vector {
    size_t value_size;
    size_t length;
    size_t capacity;
    uint8_t v[];
} Vector;

Vector *create_vec(size_t len, size_t value_size);

// Push element, grows with *3/2, ups to 3 beforehand if less than 4
uint32_t push_elem_vec(Vector **v, const void *value);

// Checked at, returns 0 if not found
void *at_vec(Vector *v, size_t idx);

// Checked memcpy, returns 0 if not found
uint32_t get_elem_vec(Vector *v, size_t idx, void *value);

// Unchecked memcpy
void get_nocheck_elem_vec(Vector *v, size_t idx, void *value);

void delete_vec(Vector *v);

#endif // VEC_H
