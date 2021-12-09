#include "queue.h"
#include "rabbit.h"

#include "mem.h"

// debug
static int g_nQueue = 0;
static int g_mQueue = 0;

struct Queue {
	rabbit * r;

	void * list;
	int used;
	int size;

	int elem_size;

	int first;
	int end;
};

#define get_node(q,s,n) cast(void *, (cast(char*,q) + s * n))

Queue * rbtQ_init( rabbit * r, size_t elem_size, size_t nelem )
{
	Queue * q = RMALLOC(r, Queue, 1);

	q->r = r;

	q->elem_size = elem_size;

	nelem = max(nelem, 32);

	q->list = rbtM_realloc(r, NULL, 0, elem_size * nelem);

	q->used = 0;
	q->size = nelem;

	q->first = 0;
	q->end = 0;

	r->obj++;
	g_nQueue++;
	g_mQueue += rbtD_queue_mem(q);

	return q;
}

void rbtQ_free(struct Queue * q)
{
	rabbit * r = q->r;

	r->obj--;
	g_nQueue--;
	g_mQueue -= rbtD_queue_mem(q);

	RFREEVECTOR(q->r, (char*)(q->list), q->elem_size * q->size);

	RFREE(q->r, q);
}

int rbtQ_empty( Queue * q )
{
	return q->first == q->end;
}

static void resize( Queue * q )
{
	void * old_list = q->list;
	int old_size = q->size;
	int old_first = q->first;
	int old_end = q->end;

	int new_size = q->size * 2 + 1;
	g_mQueue += (new_size - q->size) * q->elem_size;

	q->size = new_size;

	q->list = rbtM_realloc(q->r, NULL, 0, q->elem_size * q->size);

	q->first = q->end = 0;
	q->used = 0;
	while( old_first != old_end ) {
		memcpy(rbtQ_push(q), get_node(old_list, q->elem_size, old_first), q->elem_size);
		old_first ++;
		if(old_first >= old_size) old_first = 0;
	}

	RFREEVECTOR(q->r, old_list, old_size * q->elem_size);
}

void * rbtQ_push( Queue * q )
{
	if(q->used >= q->size - 1) {
		resize(q);
	}

	void * p = get_node(q->list, q->elem_size, q->end);

	q->end++;
	if(q->end >= q->size) q->end = 0;

	q->used ++;

	return p;
}

void * rbtQ_peek( Queue * q )
{
	if(rbtQ_empty(q)) return NULL;

	return get_node(q->list, q->elem_size, q->first);
}

void rbtQ_pop( Queue * q )
{
	if(q->first == q->end) {
		return;
	}

	q->first++;
	if(q->first >= q->size) q->first = 0;

	q->used --;
}

int rbtQ_clear( Queue * q )
{
	q->first = q->end = 0;
	q->used = 0;

	return 0;
}

int rbtQ_count( Queue * q )
{
	return q->used;
}

int rbtD_queue( const Queue * q )
{
	return 0;
}

int rbtD_queue_mem(const Queue * q)
{
	return sizeof(struct Queue) + q->size * q->elem_size;
}

int rbtQ_debug_count()
{
	return g_nQueue;
}
int rbtQ_debug_mem()
{
	return g_mQueue;
}
