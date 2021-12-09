#include "io.h"
#include "rabbit.h"
#include "mem.h"

struct io_debug g_Debug = { 0 };

struct simple_io {
	struct i_io i_if;

	rabbit * r;

	char * buf;
	int size;
	int ptr;
};

static char 		read_char	( struct i_io * io );
static short int	read_short	( struct i_io * io );
static int		read_int	( struct i_io * io );
static int		read_len	( struct i_io * io, char * out, int len );
static int		read_len_io	( struct i_io * io, struct i_io * out, int len );

static int		write_char	( struct i_io * io, char c );
static int		write_short	( struct i_io * io, short int i );
static int		write_int	( struct i_io * io, int i );
static int		write_len	( struct i_io * io, const char * in, int len );
static int		write_len_io	( struct i_io * io, struct i_io * in, int len );

static void		seek		( struct i_io * io, int at );
static int		tell		( struct i_io * io );
static int		eof		( struct i_io * io );
static int		size		( struct i_io * io );

struct i_io * io_create( rabbit * r )
{
	struct simple_io * io = RMALLOC(r, struct simple_io, 1);
	io->r = r;

	io->size = 16;
	io->ptr = 0;
	io->buf = RMALLOC(r, char, io->size);

	io->i_if.read_char = read_char;
	io->i_if.read_short = read_short;
	io->i_if.read_int = read_int;
	io->i_if.read_len = read_len;
	io->i_if.read_len_io = read_len_io;
	io->i_if.write_char = write_char;
	io->i_if.write_short = write_short;
	io->i_if.write_int = write_int;
	io->i_if.write_len = write_len;
	io->i_if.write_len_io = write_len_io;
	io->i_if.seek = seek;
	io->i_if.tell = tell;
	io->i_if.eof = eof;
	io->i_if.size = size;

	g_Debug.nio ++;
	g_Debug.mio += sizeof(struct simple_io) + io->size;

	return &io->i_if;
}

const char * io_get_p( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	return sio->buf;
}

void io_destroy( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	
	g_Debug.nio--;
	g_Debug.mio -= sizeof(struct simple_io) + sio->size;

	RFREEVECTOR(sio->r, sio->buf, sio->size);
	RFREE(sio->r, sio);
}

static char read_char( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 1 > sio->size) {
		return 0;
	}
	return sio->buf[sio->ptr++];
}

static short int read_short( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 2 > sio->size) {
		return 0;
	}

	short int i = sio->buf[sio->ptr] | (sio->buf[sio->ptr + 1] << 8);
	sio->ptr += 2;
	return i;
}

static int read_int( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 4 > sio->size) {
		return 0;
	}

	int i = sio->buf[sio->ptr] | (sio->buf[sio->ptr + 1] << 8) | (sio->buf[sio->ptr + 2] << 16) | (sio->buf[sio->ptr + 3] << 24);
	sio->ptr += 4;
	return i;
}

static int read_len( struct i_io * io, char * out, int len )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	int l = min(len, sio->size - sio->ptr);
	memcpy(out, &sio->buf[sio->ptr], l);
	sio->ptr += l;
	return l;
}

static int read_len_io( struct i_io * io, struct i_io * out, int len )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	int l = min(len, sio->size - sio->ptr);
	out->write_len(out, &sio->buf[sio->ptr], l);
	sio->ptr += l;
	return l;
}

static int expand( struct simple_io * sio, int size )
{
	int d = (1 << 6) - 1;
	size = (size + d) & (~d);
	sio->buf = RREALLOC(sio->r, char, sio->buf, sio->size, sio->size + size);
	if(!sio->buf) {
		return -1;
	}
	sio->size += size;

	g_Debug.mio += size;
	return 0;
}

static int write_char( struct i_io * io, char c )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 1 > sio->size) {
		expand(sio, 1);
	}
	sio->buf[sio->ptr++] = c;

	return 0;
}

static int write_short( struct i_io * io, short int i )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 2 > sio->size) {
		expand(sio, 2);
	}
	sio->buf[sio->ptr] = i & 0xFF;
	sio->buf[sio->ptr + 1] = (i >> 8) & 0xFF;
	sio->ptr += 2;

	return 0;
}

static int write_int( struct i_io * io, int i )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + 4 > sio->size) {
		expand(sio, 4);
	}
	sio->buf[sio->ptr] = i & 0xFF;
	sio->buf[sio->ptr + 1] = (i >> 8) & 0xFF;
	sio->buf[sio->ptr + 2] = (i >> 16) & 0xFF;
	sio->buf[sio->ptr + 3] = (i >> 24) & 0xFF;

	sio->ptr += 4;
	return 0;
}

static int write_len( struct i_io * io, const char * in, int len )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + len > sio->size) {
		expand(sio, len);
	}
	memcpy(&sio->buf[sio->ptr], in, len);
	sio->ptr += len;

	return len;
}

static int write_len_io( struct i_io * io, struct i_io * in, int len )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr + len > sio->size) {
		expand(sio, len);
	}
	in->read_len(in, &sio->buf[sio->ptr], len);
	sio->ptr += len;

	return len;
}

static void seek( struct i_io * io, int at )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	sio->ptr = max(0, min(at, sio->size));
}

static int tell( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	return sio->ptr;
}

static int eof( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	if(sio->ptr >= sio->size) {
		return 1;
	}
	return 0;
}

static int size( struct i_io * io )
{
	struct simple_io * sio = cast(struct simple_io *, io);
	return sio->ptr;
}

struct io_debug * io_get_debug()
{
	return &g_Debug;
}
