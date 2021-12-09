#ifndef _array_h_
#define _array_h_

#include "common.h"

struct Array {
	rabbit * r;

	int elem_size;
	int elem_alloc;

	int used;
	int total;

	char * p;
};

struct Array *
array_create(rabbit * r, int elem_size, int elem_alloc);

void
array_free(struct Array * array);

void *
array_push(struct Array * array);

void *
array_at(struct Array * array, int pos);

void
array_prepare(struct Array * array, int size);

int
array_length(struct Array * array);

void
array_clean(struct Array * array);

void
array_rm(struct Array * array, int pos);

int debug_array_num();
int debug_array_mem();

#endif

