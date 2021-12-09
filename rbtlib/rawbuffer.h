#ifndef _rawbuffer_h_
#define _rawbuffer_h_

#include "object.h"

#include "rabbit.h"

typedef struct rawbuffer {
    char *buf;
    int len;
    int pos;
}rawbuffer;

#define DEFAULT_ALLOCATE_LEN	256

extern rawbuffer*	rawbuffer_init( rabbit * r, int size );
extern void		rawbuffer_dealloc(rabbit * r, rawbuffer* buf);
extern char*		buffer_read(rawbuffer* buf, int len);
extern int		buffer_tell(rawbuffer* buf);
extern int		buffer_seek(rawbuffer* buf, int pos);
extern int		buffer_write(rabbit * r, rawbuffer* buf, char* str, int len);
extern int		buffer_dump(rabbit * r, rawbuffer * buf);

int rawbuffer_debug_count();
int rawbuffer_debug_mem();

#endif

