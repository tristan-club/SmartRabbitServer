#include "least_recently_used.h"
#include "mem.h"
#include "gc.h"
#include "rabbit.h"

#define lru_at_n(l,i) cast(void*, &l->pool[i * l->elem_size])

#define LRU_EMPTY       -1
#define LRU_IDLE        0
#define LRU_NOT_BUSY    1
#define LRU_BUSY        2

struct Lru {
	CommonHeader;

	void(*elem_traverse)(rabbit * r, void * p);

	size_t elem_size;

	size_t size;

	size_t last_pos;

	size_t * state;

	char * pool;
};

static void traverse( GCObject * obj )
{
	struct Lru * list = cast(struct Lru *, obj);

	if(!list->elem_traverse) {
		return;
	}

	int i;
	for(i = 0; i < list->size; ++i) {
		if(list->state[i] != LRU_EMPTY) {
			list->elem_traverse(list->r, lru_at_n(list, i));
		}
	}
}

static void release( GCObject * obj )
{
	struct Lru * list = cast(struct Lru *, obj);

	RFREEVECTOR(list->r, list->state, list->size);
	RFREEVECTOR(list->r, list->pool, list->size * list->elem_size);

	RFREE(list->r, list);
}

#define LRU_DEFAULT_SIZE	256

struct Lru * rbtLru_init( rabbit * r, size_t elem_size, size_t size )
{
//	assert(0);
	if(size < 1) {
		size = LRU_DEFAULT_SIZE;
	}
	if(elem_size < 1) {
		kLOG(r, 0, "LRU : Init. Elem Size(%zu) is illegel\n", elem_size);
		return NULL;
	}

	struct Lru * list = RMALLOC(r, struct Lru, 1);

	rbtC_link(r, cast(GCObject*, list), TLRU);

	list->gc_traverse = traverse;

	list->gc_release = release;

	list->elem_traverse = NULL;

	list->elem_size = elem_size;
	list->size = size;

	list->last_pos = -1;
	list->state = RMALLOC(r, size_t, list->size);
	memset(list->state, LRU_EMPTY, sizeof(size_t) * list->size);

	list->pool = RMALLOC(r, char, elem_size * size);
	memset(list->pool, 0, elem_size * size);

	return list;
}

void rbtLru_traverse( struct Lru * list, void (* f)( rabbit * r, void * p ) )
{
	list->elem_traverse = f;
}

void * rbtLru_push( struct Lru * list )
{
	int last_pos = list->last_pos;
	if(last_pos == -1) {
		// 刚开始
		list->last_pos = 0;
		list->state[0] = LRU_NOT_BUSY;
		return lru_at_n(list, 0);
	}

	while(1) {
		last_pos = (last_pos + 1) % list->size;
		if(list->state[last_pos] == LRU_EMPTY) {
			break;
		}

		if(list->state[last_pos] == LRU_IDLE) {
			break;
		}

		if(list->state[last_pos] == LRU_NOT_BUSY) {
			list->state[last_pos] = LRU_IDLE;
			continue;
		}

		if(list->state[last_pos] == LRU_BUSY) {
			list->state[last_pos] = LRU_NOT_BUSY;
		}
	}

	list->last_pos = last_pos;

	list->state[last_pos] = LRU_NOT_BUSY;

	return lru_at_n(list, last_pos);
}

void rbtLru_visit( struct Lru * list, void * p )
{
	if( cast(size_t, p) < cast(size_t, list->pool) || cast(size_t, p) > cast(size_t, &list->pool[list->size-1]) ) {
		// 指针不正确
		kLOG(list->r, 0, "LRU : Visit. Pointer is invalid:%p. Pool(%p - %p)\n", p, list->pool, &list->pool[list->size-1]);
		return;
	}

	int offset = cast(size_t, p) - cast(size_t, list->pool);

	int pos = offset / list->elem_size;

	if(list->state[pos] == LRU_IDLE) {
		list->state[pos] = LRU_NOT_BUSY;
		return;
	}

	if(list->state[pos] == LRU_NOT_BUSY) {
		list->state[pos] = LRU_BUSY;
	}

	return;
}

