#include "buffer.h"

#include "rabbit.h"
#include "mem.h"
#include "gc.h"

static int g_nBuffer = 0;
static int g_mBuffer = 0;

Buffer * buffer_init( rabbit * r )
{
	Buffer * b = RMALLOC( r, Buffer, 1 );

	r->obj++;

	g_nBuffer++;
	g_mBuffer += sizeof(Buffer);

	b->r = r;

	b->used = 0;
	b->size = 0;
	b->p = NULL;

	b->pos = 0;

	return b;
}

void buffer_free(Buffer * b)
{
	rabbit * r = b->r;

	r->obj--;
	g_nBuffer--;
	g_mBuffer -= rbtD_buffer_mem(r, b);

	if(b->p) {
		RFREEVECTOR(b->r, b->p, b->size);
	}
	RFREE(b->r, b);
}

int buffer_prepare_append( rabbit * r, Buffer * b, int len )
{
	if(b->used + len < b->size) {
		return 0;
	}

	size_t old_size = b->size;
	b->size = ROUND(b->used + len, 8);
	b->p = RREALLOC(r, char, b->p, old_size, b->size);

	g_mBuffer += b->size - old_size;

	if(b->p)
		return 0;
	else 
		return -1;
}

int rbtD_buffer_mem( rabbit * r, const Buffer * b )
{
	int bsize = sizeof(*b);
	int psize = sizeof(*(b->p)) * b->size;

	return bsize + psize;
}

int buffer_debug_count()
{
	return g_nBuffer;
}

int buffer_debug_mem()
{
	return g_mBuffer;
}

