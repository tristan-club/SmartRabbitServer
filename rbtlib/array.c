#include "array.h"
#include "rabbit.h"
#include "mem.h"

static int g_nArray = 0;
static int g_mArray = 0;

struct Array * array_create(rabbit * r, int elem_size, int elem_alloc)
{
	struct Array * array = RMALLOC(r, struct Array, 1);

	array->r = r;
	array->elem_size = elem_size;
	array->elem_alloc = elem_alloc;
	array->used = 0;
	array->total = elem_alloc;

	if(r) {
		array->p = RMALLOC(r, char, elem_size * elem_alloc);
		r->obj++;
	} else {
		kLOG(r, 0, "警告：Array 不是通过r分配的！\n");
		exit(1);
		array->p = (char*)malloc(elem_size * elem_alloc);
	}

	memset(array->p, 0, elem_size * elem_alloc);

	g_nArray++;
	g_mArray += sizeof(struct Array) + elem_size * elem_alloc;

	return array;
}

void array_free(struct Array * array)
{
	g_nArray--;
	g_mArray -= sizeof(struct Array) + array->elem_size * array->total;

	if(array->r) {
		array->r->obj--;
		RFREEVECTOR(array->r, array->p, array->elem_size * array->total);
		RFREE(array->r, array);
	} else {
		free(array->p);
		free(array);
	}
}

static void expand(struct Array * array)
{
	int size = array->total + array->elem_alloc;
	char * p;
        if(array->r) {
		p = RMALLOC(array->r, char, size * array->elem_size);
	} else {
		p = (char*)malloc(size * array->elem_size);
	}

	memset(p, 0, size * array->elem_size);

	memcpy(p, array->p, array->total * array->elem_size);

	if(array->r) {
		RFREEVECTOR(array->r, array->p, array->total * array->elem_size);
	} else {
		free(array->p);
	}

	array->p = p;

	array->total = size;

	g_mArray += array->elem_alloc * array->elem_size;
}

void * array_push(struct Array * array)
{
	if(array->used >= array->total) {
		expand(array);
	}

	array->used++;

	return array_at(array, array->used - 1);
}

void * array_at(struct Array * array, int pos)
{
	if(pos < 0 || pos >= array->total) {
		return NULL;
	}

	return &array->p[array->elem_size * pos];
}

void array_prepare(struct Array * array, int size)
{
	while(size > array->total) {
		expand(array);
	}
}

int array_length(struct Array * array)
{
	return array->used;
}

void array_rm(struct Array * array, int pos)
{
	if(pos < 0 || pos >= array->used) {
		return;
	}

	while(pos < array->used - 1) {
		memcpy(array_at(array, pos), array_at(array, pos + 1), array->elem_size);
		pos++;
	}

	array->used--;
}

void array_clean(struct Array * array)
{
	array->used = 0;
}

int debug_array_num()
{
	return g_nArray;
}

int debug_array_mem()
{
	return g_mArray;
}
