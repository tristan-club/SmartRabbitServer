#include "stream.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "string.h"

#include "common.h"
#include "rabbit.h"
#include "mem.h"
#include "gc.h"

static void traverse( GCObject * obj )
{
	stream * file = cast(stream *, obj);

	if(file->filename) {
		rbtC_mark(cast(GCObject *, file->filename));
	}
}

static void release( GCObject * obj )
{
	stream * file = cast(stream *, obj);

	if(file->p) {
		munmap((void*)file->p,file->size);
		file->p = NULL;
	}

	RFREE(file->r,file);
}

stream * stream_open( rabbit * r, const char * file )
{
	struct stat st;
	int fd;
	stream * f = RMALLOC(r, stream, 1);
	rbtC_link(r, cast(GCObject*, f), TSTREAM);

	f->gc_traverse = traverse;

	f->gc_release = release;

	f->p = NULL;

	if( -1 == stat(file, &st) ) {
//		perror("stream open error");
		kLOG(NULL, 0,"[Error]stream open:%s, stat error.\n",file);
		return NULL;
	}

	f->size = st.st_size;

	if( -1 == (fd = open(file, O_RDONLY)) ) {
//		perror("stream open error");
		kLOG(NULL, 0,"[Error]stream open:%s, open file error.\n",file);
		return NULL;
	}
	
	f->p = mmap(0, f->size, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);

	if(MAP_FAILED == f->p) {
	//	perror("stream open error");
		kLOG(NULL, 0,"stream open:%s, mmap error.\n",file);
		return NULL;
	}

	f->filename = rbtS_new(r, file);

	return f;
}

