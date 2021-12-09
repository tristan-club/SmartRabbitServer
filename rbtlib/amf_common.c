#include "amf_common.h"
#include "io.h"

#include "mem.h"

struct EncoderObj en_obj = { NULL, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0} };

static void idx_init( rabbit * r, idxbuffer * self )
{
	self->len = 0;
	self->list = NULL;

	self->pos = 0;
}

static void idx_dealloc(rabbit * r, idxbuffer *self)
{
	if(self->list) {
		RFREEVECTOR(r,self->list,self->len);
	}
	self->list = NULL;
	self->len = self->pos = 0;
}

int idx_map(rabbit * r, idxbuffer *self, TValue *obj)
{
	int new_len = self->pos;
	int current_len = self->len;

	if (new_len >= current_len) {
		current_len = (new_len + 1 + 3) &(~3);
	}

	if (current_len != self->len) {
		self->list = RREALLOC(r, TValue, self->list, self->len, current_len);
		int i;
		for(i = self->len; i < current_len; ++i) {
			setnilvalue(&self->list[i]);
		}

		self->len = current_len;
	}

	self->list[new_len] = *obj;
	self->pos = new_len + 1;
	return new_len;
}

TValue* idx_ret_index(rabbit * r, idxbuffer *self, int idx)
{
	if (idx >= self->pos) {
		return NULL;
	}

	TValue *result = &self->list[idx];
	return result;
}

int idx_ret_value(rabbit * r, idxbuffer* self, TValue* obj)
{
	int i;
	for(i = 0; i < self->pos; ++i) {
		if(rbtO_rawequ(&self->list[i],obj)) {
			return i;
		}
	}
	return -1;
}

void decoder_init(rabbit * r, DecodeCTX * self)
{
	self->io = NULL;

	idx_init(r, &self->obj_refs);
	idx_init(r, &self->string_refs);

	self->traits_refs.list = NULL;
	self->traits_refs.used = 0;
	self->traits_refs.size = 0;
}

void decoder_dealloc(rabbit * r, DecodeCTX *self)
{
	idx_dealloc(r,&self->obj_refs);
	idx_dealloc(r,&self->string_refs);

	int i;
	for(i = 0; self->traits_refs.list && i < self->traits_refs.used; ++i) {
		struct Traits * traits = &self->traits_refs.list[i];
		if(traits->list) {
			RFREEVECTOR(r, traits->list, traits->size);
		}
	}

	if( self->traits_refs.list ) {
		RFREEVECTOR(r, self->traits_refs.list, self->traits_refs.size);
	}
}

EncoderObj* encoder_init(rabbit * r)
{
	EncoderObj *self = &en_obj;

	self->obj_refs.pos = 0;
	self->string_refs.pos = 0;
	self->class_refs.pos = 0;

	return self;
}

void encoder_dealloc(rabbit * r, EncoderObj * en)
{
	assert(en == &en_obj);
	idx_dealloc(r, &en->obj_refs);
	idx_dealloc(r, &en->string_refs);
	idx_dealloc(r, &en->class_refs);
}
