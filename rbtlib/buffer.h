#ifndef buffer_h_
#define buffer_h_

#include "object.h"

/*
 *      Buffer
 */
struct Buffer {
	rabbit * r;

	size_t used;
	size_t size;

	size_t pos;     // for read/write

	char * p;
};

typedef struct Buffer Buffer;


Buffer * buffer_init( rabbit * r );

void buffer_free(Buffer * b);

int buffer_prepare_append( rabbit * r, Buffer * b, int len );

int rbtD_buffer_mem( rabbit * r, const Buffer * b );

// debug

int buffer_debug_count();
int buffer_debug_mem();

#endif

