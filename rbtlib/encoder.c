#include <string.h>
#include <time.h>

#include "amf.h"
#include "amf_common.h"
#include "object.h"
#include "rabbit.h"
#include "table.h"
#include "string.h"
#include "math.h"
#include "util.h"
#include "gc.h"

#include "table_struct.h"

// COMMON
static int _encode_double		(rabbit * r, EncoderObj *context, double value);
static int encode_float			(rabbit * r, EncoderObj *context, TValue *value);
static int encode_string		(rabbit * r, EncoderObj *context, TValue *value);


// AMF3
static int encode_amf3			(rabbit * r, EncoderObj * context, TValue * value);

static int _encode_int_amf3			(rabbit * r, EncoderObj *context, int value);
static int write_int_amf3			(rabbit * r, EncoderObj *context, TValue *value);
static int encode_none_amf3			(rabbit * r, EncoderObj *context);
static int encode_bool_amf3			(rabbit * r, EncoderObj *context, TValue *value);

static int serialize_string_amf3		(rabbit * r, EncoderObj *context, TValue *value);
//static int serialize_list_amf3			(rabbit * r, EncoderObj *context, TValue *value);
//static int encode_list_amf3			(rabbit * r, EncoderObj *context, TValue *value);
//static int encode_associate_array		(rabbit * r, EncoderObj* context,TValue* value);
//static int encode_reference_amf3		(rabbit * r, EncoderObj *context, idxbuffer *ref_context, TValue *value, int bit);

static int _encode_double(rabbit * r, EncoderObj *context, double value)
{
	union aligned {
		double d_value;
		char c_value[8];
	} d_aligned;
	char *char_value = d_aligned.c_value;
	d_aligned.d_value = value;

	struct i_io * io = context->io;

	if (is_bigendian()) {
		return io->write_len(io, char_value, 8);
	} else {
		char flipped[8] = {char_value[7], char_value[6], char_value[5], char_value[4], char_value[3], char_value[2], char_value[1], char_value[0]};
		return io->write_len(io, flipped, 8);
	}
}

static int encode_float(rabbit * r, EncoderObj *context, TValue *value)
{
	if(ttisfnum(value))
	{
		double n = fnumvalue(value);
		return _encode_double(r, context, n);
	}
	return 0;
}

static int _encode_int_amf3(rabbit * r, EncoderObj *context, int value)				// *
{
	char tmp[4];
	size_t tmp_size;

	/*
	 * Int can be up to 4 bytes long.
	 *
	 * The first bit of the first 3 bytes
	 * is set if another byte follows.
	 *
	 * The integer value is the last 7 bits from
	 * the first 3 bytes and the 8 bits of the last byte
	 * (29 bits).
	 *
	 * The int is negative if the 1st bit of the 29 int is set.
	 */
	value &= 0x1fffffff; // Ignore 1st 3 bits of 32 bit int, since we're encoding to 29 bit.
	if (value < 0x80) {
		tmp_size = 1;
		tmp[0] = value;
	} else if (value < 0x4000) {
		tmp_size = 2;
		tmp[0] = (value >> 7 & 0x7f) | 0x80; // Shift bits by 7 to fill 1st byte and set next byte flag
		tmp[1] = value & 0x7f; // Shift bits by 7 to fill 2nd byte, leave next byte flag unset
	} else if (value < 0x200000) {
		tmp_size = 3;
		tmp[0] = (value >> 14 & 0x7f) | 0x80;
		tmp[1] = (value >> 7 & 0x7f) | 0x80;
		tmp[2] = value & 0x7f;
	} else if (value < 0x40000000) {
		tmp_size = 4;
		tmp[0] = (value >> 22 & 0x7f) | 0x80;
		tmp[1] = (value >> 15 & 0x7f) | 0x80;
		tmp[2] = (value >> 8 & 0x7f) | 0x80; // Shift bits by 8, since we can use all bits in the 4th byte
		tmp[3] = (value & 0xff);
	} else {
		//TVal_SetString(&amfast_EncodeError, "Int is too big to be encoded by AMF.");
		return 0;
	}

	struct i_io * io = context->io;

	return io->write_len(io, tmp, tmp_size);
}

static int write_int_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	long n = 0;
	if(ttisnum(value)) {
		n = numvalue(value);
	} else {
		return 0;
	}

	struct i_io * io = context->io;

	if (n < MAX_INT && n > MIN_INT) {
		// Int is in valid AMF3 int range.
		if (io->write_char(io, INT_TYPE) < 0) {
			return 0;
		}
		return _encode_int_amf3(r, context, n);
	} else {
		// Int is too big, it must be encoded as a double
		if (io->write_char(io, DOUBLE_TYPE) < 0) {
			return 0;
		}
		return _encode_double(r, context, (double)n);
	}
}

/* Encode a none. */
static int encode_none_amf3(rabbit * r, EncoderObj *context)
{
	struct i_io * io = context->io;

	return io->write_char(io, NULL_TYPE);
}

static int encode_bool_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	struct i_io * io = context->io;

	if (bvalue(value) != 0) {
		return io->write_char(io, TRUE_TYPE);
	} else {
		return io->write_char(io, FALSE_TYPE);
	}
}

static int serialize_string_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	struct i_io * io = context->io;

	TString * ts = strvalue(value);
	if (rbtS_len(ts) == 0) {
		return io->write_char(io, EMPTY_STRING_TYPE);
	}

	return encode_string(r,context, value);
}

/* Encode a String as UTF8. */
static int encode_string(rabbit * r, EncoderObj *context, TValue *value)
{
	/*
	 *  TODO: The following code assumes all strings
	 *  are already UTF8 or ASCII.
	 *
	 *  Should we check to make sure,
	 *  or just let the client pick it up?
	 */
	TString * ts = strvalue(value);
	char *char_value = cast(char *, rbtS_gets(ts));
	size_t string_len = rbtS_len(ts);
	if (!char_value) {
		return 0;
	}

	// Add size of string to header
	if (!_encode_int_amf3(r, context, ((int)string_len) << 1 | REFERENCE_BIT)) {
		return 0;
	}

	struct i_io * io = context->io;

	// Write string.
	return io->write_len(io, char_value, (int)string_len);
}

/* Serializes a array */
/*
static int serialize_list_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	// Check for idx
	int result = encode_reference_amf3(r, context, &context->obj_refs, value, 0);
	if (result > -1)
		return result;

	return encode_list_amf3(r, context, value);
}*/

/* Encode an object */
static int encode_object( rabbit * r, EncoderObj * context, TValue * value )
{
	struct i_io * io = context->io;

	io->write_char(io, OBJECT_TYPE);

	_encode_int_amf3(r, context, 0xb);
	const TString * empty_str = rbtS_new(r, "");
	TValue tv;
	setstrvalue(&tv, empty_str);
	encode_string(r, context, &tv);

	Table * tbl = tblvalue(value);
	if(!tbl) {
		tbl = rbtH_init(r, 1, 1);
	}

	const TString * Root = rbtS_new(r, "Root");

	int idx = -1;
	TValue key, val;
	while(1) {
		idx = rbtH_next(r, tbl, idx, &key, &val);
		if(idx < 0) {
			break;
		}

		if(ttisstr(&key) && rbtS_len(strvalue(&key))) {

			if(strvalue(&key) != Root) {
				encode_string(r, context, &key);
				encode_amf3(r, context, &val);
			}
			continue;
		}
		if(ttisnum(&key)) {
			char * p = utos(numvalue(&key));
			const TString * ts = rbtS_new(r, p);
			setstrvalue(&tv,ts);
			encode_string(r, context, &tv);
			encode_amf3(r, context, &val);
			continue;
		}
		if(ttisfnum(&key)) {
			char * p = ftos(fnumvalue(&key));
			const TString * ts = rbtS_new(r, p);
			setstrvalue(&tv,ts);
			encode_string(r, context, &tv);
			encode_amf3(r, context, &val);
		}
	}

	io->write_char(io, EMPTY_STRING_TYPE);

	return 1;
}

/* Encode a array. */
/*
static int encode_list_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	// Add size of list to header
	size_t value_len = rbtH_countnum(r,tblvalue(value));
	if (value_len < 0) {
		return 0;
	}

	if (!_encode_int_amf3(r, context, ((int)value_len) << 1 | REFERENCE_BIT)) {
		return 0;
	}

	if(!encode_associate_array(r,context,value)){
		return 0;
	}

	// Encode each value in the list
	int i;
	Table * t = tblvalue(value);
	for (i = 0; i < t->vector_size; ++i) {
		TValue * list_item = &t->vector[i];
		if (ttisnil(list_item)) {
			continue;
		}

		int result = encode_amf3(r, context, list_item);

		if (!result)
			return 0;
	}

	return 1;
}*/

/*
static int encode_associate_array(rabbit * r, EncoderObj* context,TValue* value)
{
	int i;
	Table * t = tblvalue(value);
	for(i = 0; i < t->table_size; ++i) {
		struct Node * n = gnode(t,i);
		if(ttisstr(gkey(n))) {
			serialize_string_amf3(r, context,gkey(n));
			if(!encode_amf3(r, context, gval(n))) {
				return 0;
			}
		}
	}
	context->io->write_char(context->io, EMPTY_STRING_TYPE);

	return 1;
}*/

/*
static int encode_reference_amf3(rabbit * r, EncoderObj *context, idxbuffer *ref_context, TValue *value, int bit)
{
	// Using references is an option set in the context
	int idx = idx_ret_value(r, ref_context, value);
	if (idx > -1) {
		if (idx < MAX_INT) {// Max reference count
			if (!_encode_int_amf3(r, context, (idx << (bit + 1)) | (0x00 + bit)))
				return 0;
			return 1;
		}
	}

	// Object is not indexed, add index
	if (idx_map(r, ref_context, value) == -1)
		return 0;

	return -1;
}*/

/* Encode a Python object that doesn't have a C API in AMF3. */
static int encode_unknown_object_amf3(rabbit * r, EncoderObj *context, TValue *value)
{
	return context->io->write_char(context->io, UNDEFINED_TYPE);
}

/*
 *	检查传入的 io 是否有效：
 *		
 *	-- 必须实行 write_char 和 write_len
 *
 */
static int check_valid_io(struct i_io * in)
{
	if(	!in ||
		!in->write_char ||
		!in->write_len
	  ) {
		return -1;
	}
	return 0;
}

int rbtAMF_encode(rabbit * r, const TValue * value, struct i_io * in)
{
	if(check_valid_io(in) < 0) {
		kLOG(r, 0, "[Warning] %s:%d 传入的in(%p)没有实行write_char 或 write_len!\n", __FUNCTION__, __LINE__, in);
		return -1;
	}

	rbtStat_rt_start(r, amf3_encode);

	EncoderObj * context = encoder_init(r);
	context->io = in;

	int ret = encode_amf3(r, context, cast(TValue *, value));

	encoder_dealloc(r, context);

	rbtStat_rt_end(r, amf3_encode);

	return ret;
}

static int encode_amf3(rabbit * r, EncoderObj * context, TValue * value)
{
	if(ttisbool(value)) {
		return encode_bool_amf3(r, context, value);
	}

	struct i_io * io = context->io;

	switch (ttype(value)){
		case TNIL:
			return encode_none_amf3(r, context);break;

		case TNUMBER:
			return write_int_amf3(r,context, value);break;

		case TFLOAT:
			if (context->io->write_char(context->io, DOUBLE_TYPE) < 0) {
				return 0;
			}
			return encode_float(r, context, value); break;

		case TSTRING:
			if (io->write_char(io, STRING_TYPE) < 0){
				return 0;
			}
			return serialize_string_amf3(r, context, value); break;

		case TTABLE:
			return encode_object(r, context, value);break;

		default:
			return encode_unknown_object_amf3(r,context, value); break;
	}
}

