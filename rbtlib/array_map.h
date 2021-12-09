#ifndef array_map_h_
#define array_map_h_

#ifdef DEBUG_ARRAY_MAP

#define ONLY_LIST
typedef struct rabbit rabbit;

#else

#include "common.h"
#include "mem.h"
#include "rabbit.h"

#endif

#include "list.h"

struct array_map {
	rabbit * r;
	char * p;
	int node_size;
	int elem_size;
	int elem_count;
	struct list_head head_busy;
	struct list_head head_idle;
};

#define array_map_busy_head(am)	&((am)->head_busy)

struct array_map *
array_map_create(rabbit * r, int elem_size, int elem_count);

void
array_map_destroy(struct array_map * am);

int
array_map_push(struct array_map * am, int pos);

int
array_map_is_empty(struct array_map * am, int pos);

void *
array_map_at(struct array_map * am, int pos);

int
array_map_id(struct array_map * am, void * data);

void
array_map_free(struct array_map * am, int pos);

#define array_map_free_p(am, p)	array_map_free(am, array_map_id(am, p))

void *
array_map_list_to_value(struct array_map * am, struct list_head * p);

/*
 *	int id = -1;
 *	while(1) {
 *		int pos = array_map_next(am, &id);
 *		if(pos < 0) {
 *			break;
 *		}
 *		void * p = array_map_at(am, pos);
 *		do_something(p);
 *	}
 *	在array_next中，可以调用array_map_free(am, pos);
 */
int 
array_map_next(struct array_map * am, int * id);

int
array_map_reverse_next(struct array_map * am, int * id);

#endif

