/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <stdlib.h>

// Generic Vector Type
typedef struct Vector {
    size_t value_size;
    size_t length;
    size_t capacity;
    uint8_t v[];
} Vector;

Vector *create_vec(size_t len, size_t value_size);

// Push element, grows with *3/2, ups to 3 beforehand if less than 4
uint32_t push_elem_vec(Vector **v, const void *value);

// Unchecked peek, returns the address, must contain more than 0 elements
void *peek_elem_vec(Vector *v);

// Unchecked pop, must contain more than 0 elements
void pop_elem_vec(Vector *v);

// Unchecked at, returns the address, must contain the elem at idx
void *at_elem_vec(Vector *v, size_t idx);

// Unchecked memcpy, must contain the elem at idx
void get_elem_vec(Vector *v, size_t idx, void *value);

void delete_vec(Vector *v);

// String Vector Type
typedef struct String {
    size_t length;
    size_t capacity;
    char s[];
} String;

String *create_str(size_t len);

// Create a str using an existing cstr
String *from_cstr(char *cs);

// Push char, grows with *3/2, ups to 3 beforehand if less than 4
uint32_t push_elem_str(String **s, char value);

// Unchecked peek, must contain more than 0 elements
char peek_elem_str(String *s);

// Unchecked pop, must contain more than 0 elements
void pop_elem_str(String *s);

// push_elem, just in a single batch
uint32_t push_slice_str(String **s, char *start, size_t len);

void delete_str(String *s);

#endif // VEC_H
