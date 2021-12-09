#ifndef mblock_h_
#define mblock_h_

#include "io.h"

struct MBlock;

struct MBlockIO {
	struct i_io i_if;

	rabbit * r;

	struct MBlock * block;
	int start;

	int ptr;
	int size;

	struct MBlock * mcurr;
	char * pcurr;

	struct MBlock ** pend;

	int error;
};

struct MBlock *
mblock_create( rabbit * r );

void
mblock_free( struct MBlock * block );

void
mblock_free_list( struct MBlock * block );

void
mblock_link( struct MBlock * from, struct MBlock * to );

struct i_io *
mblock_io( struct MBlockIO * mio, struct MBlock * block, int start );

struct MBlock **
mblock_next(struct MBlock * block);

// Debug
int
mblock_debug_mem();

int
mblock_debug_count();

#endif

