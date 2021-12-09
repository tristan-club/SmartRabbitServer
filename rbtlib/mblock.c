#include "mblock.h"
#include "util.h"
#include "rabbit.h"
#include "mem.h"

#define MBlockSize	2046

static struct MBlock * g_List = NULL;
static int g_nBlk = 0;

int
mblock_debug_mem()
{
	return g_nBlk * MBlockSize;
}

int
mblock_debug_count()
{
	return g_nBlk;
}

struct MBlock **
mblock_next( struct MBlock * block )
{
	char * p = cast(char *, block);
	return cast(struct MBlock **, &p[MBlockSize - sizeof(void *)]);
}


struct MBlock *
mblock_create( rabbit * r )
{
	struct MBlock * block = NULL;

	if(!g_List) {
		char * p = RMALLOC(r, char, MBlockSize);
		if(!p) {
			return NULL;
		}
		g_List = cast(struct MBlock *, p);
		*(mblock_next(g_List)) = NULL;
		g_nBlk++;
	}

	block = g_List;
	g_List = *(mblock_next(block));

	mblock_link(block, NULL);

	return block;
}

void
mblock_free(struct MBlock * block)
{
	*(mblock_next(block)) = g_List;
	g_List = block;
}

void
mblock_free_list( struct MBlock * block )
{
	while(block) {
		struct MBlock ** pnext = mblock_next(block);
		struct MBlock * next = *pnext;
		mblock_free(block);
		block = next;
	}
}

void
mblock_link(struct MBlock * from, struct MBlock * to)
{
	*(mblock_next(from)) = to;
}

//------------------ MBlock IO ------------------------
static char 		read_char		(struct i_io * io);
static short int 	read_short		(struct i_io * io);
static int		read_int		(struct i_io * io);
static int 		read_len		(struct i_io * io, char * out, int len);
static int 		read_len_io		(struct i_io * io, struct i_io * out, int len);
static int		write_char		(struct i_io * io, char c);
static int		write_short		(struct i_io * io, short int i);
static int		write_int		(struct i_io * io, int i);
static int		write_len		(struct i_io * io, const char * in, int len);
static int		write_len_io		(struct i_io * io, struct i_io * in, int len);
static void		seek			(struct i_io * io, int at);
static int		tell			(struct i_io * io);
static int		eof			(struct i_io * io);
static int		size			(struct i_io * io);
static void		erase			(struct i_io * io, int len);
static int		error			(struct i_io * io);
static void		encode			(struct i_io * io, int len);


struct i_io *
mblock_io(struct MBlockIO * mio, struct MBlock * block, int start)
{
	mio->block = block;
	mio->start = start;
	mio->size = 0;
	mio->error = 0;

	mio->i_if.read_char = read_char;
	mio->i_if.read_short = read_short;
	mio->i_if.read_int = read_int;
	mio->i_if.read_len = read_len;
	mio->i_if.read_len_io = read_len_io;
	mio->i_if.write_char = write_char;
	mio->i_if.write_short = write_short;
	mio->i_if.write_int = write_int;
	mio->i_if.write_len = write_len;
	mio->i_if.write_len_io = write_len_io;
	mio->i_if.seek = seek;
	mio->i_if.tell = tell;
	mio->i_if.eof = eof;
	mio->i_if.size = size;
	mio->i_if.erase = erase;
	mio->i_if.error = error;
	mio->i_if.encode = encode;

	seek(&mio->i_if, 0);

	return &mio->i_if;
}

static inline int _next_block(struct MBlockIO * mio)
{
	struct MBlock ** pend = mio->pend;

	if(*pend) {
		mio->mcurr = *pend;
		mio->pcurr = cast(char *, mio->mcurr);
		mio->pend = mblock_next(mio->mcurr);

		return 0;
	}
	return -1;
}

// 读取下一个字节，并将内部指针后移一位，**必须可以读取成功**
static char _next(struct MBlockIO * mio)
{
	if(mio->pcurr >= cast(char *, mio->pend)) {
		int r = _next_block(mio);
		used(r);
		assert(r == 0);
	}
	mio->ptr++;
	char c = *mio->pcurr;
	mio->pcurr++;
	return c;
}

static char read_char(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(mio->ptr + 1 > mio->size) {
		mio->error = IO_ERROR_READ_EOF;
		return 0;
	}

	return _next(mio);
}

static short int read_short(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(mio->ptr + 2 > mio->size) {
		mio->error = IO_ERROR_READ_EOF;
		return 0;
	}

	if(is_bigendian()) {
		return ((_next(mio) << 8) & 0xFF00) | (_next(mio) & 0xFF);
	}
	return (_next(mio) & 0xFF) | ((_next(mio) << 8) & 0xFF00);
}

static int read_int(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(mio->ptr + 4 > mio->size) {
		mio->error = IO_ERROR_READ_EOF;
		return 0;
	}

	if(is_bigendian()) {
		return ((_next(mio) << 24) & 0xFF000000) | ((_next(mio) << 16) && 0xFF0000) | ((_next(mio) << 8) & 0xFF00) | (_next(mio) & 0xFF);
	}
	return (_next(mio) & 0xFF) | ((_next(mio) << 8) & 0xFF00) | ((_next(mio) << 16) & 0xFF0000) | ((_next(mio) << 24) & 0xFF000000);
}

static int read_len( struct i_io * io, char * out, int len )
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;

	if(len < 0) {
		mio->error = IO_ERROR_BAD_PARAM;
		return 0;
	}

	int rlen = min(mio->size - mio->ptr, len);
	int clen = rlen;

	while(clen) {
		int size = cast(char *, mio->pend) - mio->pcurr;
		if(size <= 0) {
			int tmp = _next_block(mio);
			used(tmp);
			assert(tmp == 0);
			continue;
		}
		int count = min(clen, size);
		memcpy(out, mio->pcurr, count);
		clen -= count;
		mio->pcurr += count;
		mio->ptr += count;
		out += count;
	}

	return rlen;
}

static int read_len_io( struct i_io * io, struct i_io * out, int len )
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;

	if(len < 0) {
		mio->error = IO_ERROR_BAD_PARAM;
		return 0;
	}

	int rlen = min(mio->size - mio->ptr, len);
	int clen = rlen;

	while(clen) {
		int size = cast(char *, mio->pend) - mio->pcurr;
		if(size <= 0) {
			int tmp = _next_block(mio);
			used(tmp);
			assert(tmp == 0);
			continue;
		}
		int count = min(clen, size);
		if(out->write_len(out, mio->pcurr, count) < 0) {
			mio->error = out->error(out);
			return mio->error;
		}

		clen -= count;
		mio->pcurr += count;
		mio->ptr += count;
	}

	return rlen;
}

static int _write(struct MBlockIO * mio, char c)
{
	struct MBlock ** pend = mio->pend;
	if(mio->pcurr >= cast(char *, pend)) {
		if(!(*pend)) {
			*pend = mblock_create(mio->r);
			if(!(*pend)) {
				mio->error = IO_ERROR_WRITE_NO_MEM;
				return -1;
			}
		}
		_next_block(mio);
	}
	*(mio->pcurr) = c;
	mio->pcurr++;
	mio->ptr++;
	mio->size = max(mio->size, mio->ptr);
	return 0;
}

static int write_char( struct i_io * io, char c )
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	return _write(mio, c);
}

static int write_short(struct i_io * io, short int i)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(is_bigendian()) {
		_write(mio, (i >> 8) & 0xFF);
		_write(mio, i & 0xFF);
	} else {
		_write(mio, i & 0xFF);
		_write(mio, (i >> 8) & 0xFF);
	}
	return mio->error;
}

static int write_int(struct i_io * io, int i)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(is_bigendian()) {
		_write(mio, (i >> 24) & 0xFF);
		_write(mio, (i >> 16) & 0xFF);
		_write(mio, (i >> 8) & 0xFF);
		_write(mio, i & 0xFF);
	} else {
		_write(mio, i & 0xFF);
		_write(mio, (i >> 8) & 0xFF);
		_write(mio, (i >> 16) & 0xFF);
		_write(mio, (i >> 24) & 0xFF);
	}
	return mio->error;
}

static int write_len(struct i_io * io, const char * in, int len)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	int left = len;

	while(left) {
		int size = cast(char *, mio->pend) - mio->pcurr;
		if(size <= 0) {
			struct MBlock ** pend = mio->pend;
			if(!(*pend)) {
				*pend = mblock_create(mio->r);
				if(!(*pend)) {
					mio->error = IO_ERROR_WRITE_NO_MEM;
					return -1;
				}
			}
			_next_block(mio);

			continue;
		}
		int count = min(left, size);
		memcpy(mio->pcurr, in, count);

		left -= count;
		in += count;
		mio->pcurr += count;
		mio->ptr += count;
	}
	mio->size = max(mio->size, mio->ptr);

	return len;
}

static int write_len_io(struct i_io * io, struct i_io * in, int len)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	int left = len;

	while(left) {
		int size = cast(char *, mio->pend) - mio->pcurr;
		if(size <= 0) {
			struct MBlock ** pend = mio->pend;
			if(!(*pend)) {
				*pend = mblock_create(mio->r);
				if(!(*pend)) {
					mio->error = IO_ERROR_WRITE_NO_MEM;
					return -1;
				}
			}
			_next_block(mio);

			continue;
		}
		int count = min(left, size);
		if(in->read_len(in, mio->pcurr, count) < 0) {
			mio->error = in->error(in);
			return mio->error;
		}

		left -= count;
		mio->pcurr += count;
		mio->ptr += count;
	}
	mio->size = max(mio->size, mio->ptr);

	return len;
}

static void seek(struct i_io * io, int at)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;

	if(at > mio->size) {
		kLOG(mio->r, 0, "MBlock.seek(%d)，超出了范围(%d)!\n", at, mio->size);
		return;
	}

	mio->ptr = at;
	mio->mcurr = mio->block;

	char * p = cast(char *, mio->block);
	mio->pcurr = &p[mio->start];
	mio->pend = mblock_next(mio->block);

	while(at) {
		int size = cast(char *, mio->pend) - mio->pcurr;
		if(at <= size) {
			mio->pcurr += at;
			break;
		}
		at -= size;
		_next_block(mio);
	}
	return;
}

static int tell(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	return mio->ptr;
}

static int eof(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	if(mio->ptr >= mio->size) {
		return 1;
	}
	return 0;
}

static int size(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	return mio->size;
}

static void erase(struct i_io * io, int len)
{
	if(len < 1) {
		return;
	}

	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	mio->error = 0;
	mio->size = max(mio->size - len, 0);
	if(mio->size < mio->ptr) {
		seek(io, mio->size);
	}
}

static int error(struct i_io * io)
{
	struct MBlockIO * mio = cast(struct MBlockIO *, io);
	return mio->error;
}

static void encode(struct i_io * io, int len)
{
	int klen, default_klen = 16, special_klen = 32;
	int offset = 13;
        int flag, encodebyte = 0;

        static const char * default_key = "!#1*3)-~@*%e$x=]";
	static const char * encode_key = "r*j&y%2(0)@~;m.K'pH^[$?D7#/U|4V+";
        static char special_key[INT_SIZE * 64];
        char *key, *keystart, *pkeychar = NULL;
        char keychar = 0;

        struct MBlockIO * mio = cast(struct MBlockIO *, io);

       	if (len < 0 || len + mio->ptr > mio->size){
       		len = mio->size - mio->ptr;
       	}

       	if (mio->size <= mio->ptr + offset) {
		key = keystart = cast(char *, default_key);
               	klen = default_klen;
       	}
       	else {
                if (mio->pcurr + offset >= cast(char *, mio->pend)) {
                       if (*mio->pend) {
                                pkeychar  = cast(char *, *mio->pend) + offset - (cast(char *, mio->pend) - mio->pcurr);
                                flag = 0;
                        }
		       	else {
				flag = -1;
                      	}
		        assert(flag == 0);
		}
		else {
			pkeychar = mio->pcurr + offset;
                }

                keychar  = *pkeychar;

                int i = 0;
                for (; i < special_klen ; i++) {
                        special_key[i] = keychar ^ encode_key[i];
                }
                special_key[i] = 0;
                key = keystart = special_key;
                klen = i;
	}

	while (encodebyte < len) {
                if (mio->pcurr >= cast(char *, mio->pend)) {
                        if (*mio->pend) {
                                mio->mcurr = *mio->pend;
                                mio->pcurr = cast(char *, mio->mcurr);
                                mio->pend = mblock_next(mio->mcurr);
                                flag = 0;
                        }
                        else {
                                flag = -1;
                        }
                        assert(flag == 0);
                }

		*mio->pcurr ^= *key;

		mio->pcurr++;
		encodebyte++;

		if (key + 1 >= keystart + klen) {
			key = keystart;
		}
		else {
			key++;
		}
	}

	if (pkeychar) {
		*pkeychar = keychar;
	}
}
