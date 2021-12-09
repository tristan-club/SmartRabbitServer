#include "rawbuffer.h"

#include "mem.h"

static int g_nRawBuffer = 0;
static int g_mRawBuffer = 0;


rawbuffer* rawbuffer_init( rabbit * r, int size )
{
	rawbuffer* self = RMALLOC(r, rawbuffer, 1);

	size = max(size, 64);

	if(size > 0) {
		self->buf = (char*)rbtM_realloc(r, NULL, 0, size);
		memset(self->buf, 0, size);
	} else {
		self->buf = NULL;
		size = 0;
	}
	self->len = size;
	self->pos = 0;

	r->obj++;
	g_nRawBuffer++;
	g_mRawBuffer += sizeof(rawbuffer) + self->len;

	return self;
}

void rawbuffer_dealloc(rabbit * r, rawbuffer *self)
{
	if(!self) {
		return;
	}

	r->obj--;
	g_nRawBuffer--;
	g_mRawBuffer -= sizeof(rawbuffer);

	if(self->buf) {
		g_mRawBuffer -= self->len;
		RFREEVECTOR(r, self->buf, self->len);
	}
	RFREE(r,self);

}

int buffer_tell(rawbuffer *self)
{
	return self->pos;
}

int buffer_seek(rawbuffer *self, int pos)
{
	if(pos >= self->len) {
		return -1;
	}
	self->pos = pos;
	return pos;
}

char* buffer_read(rawbuffer *self, int len)
{
	if ((self->pos + len) < 0) {
		return NULL;
	}

	if ((self->pos + len) > self->len) {
		return NULL;
	}

	char* result = (char*)(self->buf + self->pos);
	self->pos += len;
	return result;
}

static int buffer_grow(rabbit * r, rawbuffer *self, int len)
{
	int new_len = self->pos + len;
	int current_len = self->len;

	while (new_len > current_len) {
		// Buffer is not large enough.
		// Double its memory, so that we don't need to realloc every time.
		current_len *= 2;
	}

	if (current_len != self->len) {
		self->buf = RREALLOC(r,char,self->buf,self->len,current_len);

		g_mRawBuffer += current_len - self->len;

		if (self->buf == NULL) {
			return -1;
		}
		self->len = current_len;
	}

	return current_len;
}

int buffer_write(rabbit * r, rawbuffer *self, char *str, int len)
{
	if (buffer_grow(r, self, len) == -1) {
		return 0;
	}

	memcpy(self->buf + self->pos, str, (size_t)len);
	self->pos += len;
	return 1;
}

int buffer_dump( rabbit * r, rawbuffer * buf )
{
	int i;
	for(i = 0;i < buf->pos; ++i) {
		fprintf(stderr, "%x\t", buf->buf[i]);
	}
	fprintf(stderr, "\n");

	return 0;
}

int rawbuffer_debug_count()
{
	return g_nRawBuffer;
}

int rawbuffer_debug_mem()
{
	return g_mRawBuffer;
}
