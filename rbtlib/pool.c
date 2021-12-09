#include "pool.h"
#include "mem.h"
#include "rabbit.h"

// debug
static int g_nPool = 0;
static int g_mPool = 0;

struct PoolLink {
	int next;
	int state;
};

#define POOL_SLOT_FREE  0
#define POOL_SLOT_BUSY  1

#define POOL_LINK_NULL  -1

#define PoolLink_n(pool, i) cast(struct PoolLink *, pool->pool + (pool->elem_size + sizeof(struct PoolLink)) * i)
#define PoolElem_n(pool, i) cast(char *, pool->pool + (pool->elem_size + sizeof(struct PoolLink)) * i + sizeof(struct PoolLink))

struct Pool {
	rabbit * r;

	char * pool;

	int elem_size;
	int size;

	int init_size;

	int last_free;
	int nfree;

};

void rbtPool_destroy(struct Pool * pool)
{
	rabbit * r = pool->r;

	r->obj--;
	g_nPool--;
	g_mPool -= rbtPool_mem(pool);

	RFREEVECTOR(r, pool->pool, (pool->elem_size + sizeof(struct PoolLink)) * pool->size);

	RFREE(r, pool);
}


struct Pool * rbtPool_init( rabbit * r, int elem_size, int init_size )
{
	if(init_size <= 0) {
		init_size = 16;
	}

	int size = (elem_size + sizeof(struct PoolLink)) * init_size;

	struct Pool * pool = RMALLOC(r, struct Pool, 1);

	pool->r = r;

	pool->elem_size = elem_size;
	pool->init_size = init_size;
	pool->size = init_size;
	pool->nfree = init_size;

	pool->pool = RMALLOC(r, char, size);
	memset(pool->pool, 0, size);

	struct PoolLink * pl = PoolLink_n(pool, 0);
	pl->next = POOL_LINK_NULL;
	pl->state = POOL_SLOT_FREE;

	int i;
	for(i = 1; i < init_size; ++i) {
		struct PoolLink * pl = PoolLink_n(pool, i);
		pl->next = i - 1;
		pl->state = POOL_SLOT_FREE;
	}

	pool->last_free = init_size - 1;

	r->obj++;
	g_nPool++;
	g_mPool += rbtPool_mem(pool);

	return pool;
}

static int resize( struct Pool * pool )
{
	int new_size = pool->size + pool->init_size;

	int old_size_raw = (pool->elem_size + sizeof(struct PoolLink)) * pool->size;
	int new_size_raw = (pool->elem_size + sizeof(struct PoolLink)) * new_size;

	pool->pool = RREALLOC(pool->r, char, pool->pool, old_size_raw, new_size_raw);
	memset(pool->pool + old_size_raw, 0, new_size_raw - old_size_raw);

	struct PoolLink * pl = PoolLink_n(pool, pool->size);
	pl->next = POOL_LINK_NULL;
	pl->state = POOL_SLOT_FREE;

	int i;
	for(i = pool->size + 1; i < new_size; ++i) {
		struct PoolLink * pl = PoolLink_n(pool, i);
		pl->next = i - 1;
		pl->state = POOL_SLOT_FREE;
	}

	pool->size = new_size;
	pool->last_free = pool->size - 1;
	pool->nfree = pool->init_size;

	g_mPool += new_size_raw - old_size_raw;

	return 0;
}

static int get_next_slot( struct Pool * pool )
{
	if(pool->last_free == POOL_LINK_NULL) {
		resize(pool);
	}

	int free = pool->last_free;

	struct PoolLink * pl = PoolLink_n(pool, free);

	pool->last_free = pl->next;
	pool->nfree--;

	pl->state = POOL_SLOT_BUSY;

	return free;
}

void * rbtPool_at( struct Pool * pool, int id )
{
	if(id >= 0 && id < pool->size) {
		char * p = PoolElem_n(pool, id);

		return p;
	}

	return NULL;
}

int rbtPool_push( struct Pool * pool )
{
	int slot = get_next_slot( pool );

	return slot;
}

int rbtPool_free( struct Pool * pool, int id )
{
	if(id < 0 || id >= pool->size) {
		return 0;
	}

	struct PoolLink * pl = PoolLink_n(pool, id);
	if(pl->state != POOL_SLOT_BUSY) {
		kLOG(pool->r, 0, "Pool Free. But Is Not Busy\n");
		return 0;
	}

	pl->next = pool->last_free;
	pool->last_free = id;

	pl->state = POOL_SLOT_FREE;

	pool->nfree++;

	return 0;
}

int rbtPool_count( struct Pool * pool )
{
	return (pool->size - pool->nfree);
}

int rbtPool_mem( struct Pool * pool )
{
	return sizeof(struct Pool) + (pool->size * (pool->elem_size + sizeof(struct PoolLink)));
}

int rbtPool_debug_count()
{
	return g_nPool;
}

int rbtPool_debug_mem()
{
	return g_mPool;
}

