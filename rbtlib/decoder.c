#include "amf.h"
#include "amf_common.h"

#include "object.h"
#include "string.h"
#include "rabbit.h"
#include "table.h"
#include "mem.h"
#include "gc.h"
#include "util.h"

static void decode_error(DecodeCTX * context)
{
	longjmp(context->err_jmp, 1);
}

static int decode_amf3(rabbit * r, DecodeCTX * context, TValue * v);

static int _decode_int_amf3(DecodeCTX *context);
static double _decode_double(DecodeCTX *context);

static int			decode_double			(rabbit * r, DecodeCTX *context, TValue * v);
static const TString*		decode_string			(rabbit * r, DecodeCTX *context, unsigned int string_size);
static double			decode_date			(DecodeCTX *context);

static TValue*	decode_reference_amf3		(rabbit * r, DecodeCTX *context, idxbuffer *obj_context, int val);
static int	decode_dynamic_dict_amf3	(rabbit * r, DecodeCTX *context, Table *dict);
static int	decode_int_amf3			(rabbit * r, DecodeCTX *context, TValue * v);

static int	deserialize_string_amf3		(rabbit * r, DecodeCTX *context, TValue * v);
static int	deserialize_array_amf3		(rabbit * r, DecodeCTX *context, int collection, TValue * v);
static int	decode_dynamic_array_amf3	(rabbit * r, DecodeCTX *context, Table * list_val, int array_len);

static int decode_object( rabbit * r, DecodeCTX * context, TValue * v );


/* Add the dynamic attributes of an encoded obj to a dict. */
static int decode_dynamic_dict_amf3(rabbit * r, DecodeCTX *context, Table *dict)
{
	TValue key,val;
	while (1) {
		setnilvalue(&key);
		setnilvalue(&val);
		deserialize_string_amf3(r, context, &key);
		if (ttisnil(&key)) {
			kLOG(r, 0, "Decode Dynamic Dict. Key Is Nil\n");
			decode_error(context);
			return -1;
		}

		if (rbtS_len(strvalue(&key)) == 0) {
			// Empty string marks end of name/value pairs
			return 0;
		}

		decode_amf3(r, context, &val);
		if (ttisnil(&val)) {
			return 0;
		}

		setvalue(rbtH_set(r,dict,&key),&val);
	}

	return 0;
}

static int decode_traits_reference_object( rabbit * r, DecodeCTX * context, TValue * v, int ref )
{
	TraitsList * tl = &context->traits_refs;
	if(ref < 0 || ref >= tl->used) {
		kLOG(r, 0, "Traits Ref too big : %d -- %d\n", ref, tl->used);
		setnilvalue(v);
		decode_error(context);
		return -1;
	}

	Traits * trait = &tl->list[ref];

	Table * ret = rbtH_init(r, 1, 1);

	int i; 
	TValue sealed_v;
	for(i = 0; i < trait->nsealed; ++i) {
		decode_amf3(r, context, &sealed_v);
		setvalue(rbtH_setstr(r, ret, rbtS_gets(trait->list[i])), &sealed_v);
	}

	if(trait->nsealed > 0) {
		return 0;
	}

	TValue key, val;

	deserialize_string_amf3(r, context, &key);
	if(!ttisstr(&key)) {
		return 0;
	}

	while(rbtS_len(strvalue(&key))) {
		decode_amf3(r, context, &val);
		setvalue(rbtH_setstr(r, ret, rbtS_gets(strvalue(&key))), &val);
		deserialize_string_amf3(r, context, &key);
		if(!ttisstr(&key)) {
			break;
		}
	}
	
	settblvalue(v, ret);

	idx_map(r, &context->obj_refs, v);
	
	return 0;
}

static int traits_add( rabbit * r, Traits * t, TString * ts )
{
	if(t->nsealed >= t->size) {
		size_t new_size = (t->nsealed + 1) * 2;
		t->list = RREALLOC(r, TString *, t->list, t->size, new_size);
		while(t->size < new_size) {
			t->list[t->size++] = NULL;
		}
	}

	t->list[t->nsealed++] = ts;

	return 0;
}

static Traits * new_traits( rabbit * r, TraitsList * tl )
{
	if(!tl) {
		return NULL;
	}

	if(tl->used >= tl->size) {
		size_t new_size = (tl->used + 1) * 2;
		tl->list = RREALLOC(r, Traits, tl->list, tl->size, new_size);
		while(tl->size < new_size) {
			tl->list[tl->size].list = NULL;
			tl->list[tl->size].nsealed= 0;
			tl->list[tl->size].size = 0;
			tl->list[tl->size].dynamic = 0;

			tl->size++;
		}
	}

	return &tl->list[tl->used++];
}

static int decode_object( rabbit * r, DecodeCTX * context, TValue * v )
{
	int header = _decode_int_amf3( context );

	TValue * ref = decode_reference_amf3(r,context,&context->obj_refs, header);

	if(ref && !ttisnil(ref)) {
		setvalue(v,ref);
		return 0;
	}

	if( (header & 0x2) == 0 ){	// == 1 为AS3的Object类型
		return decode_traits_reference_object( r, context, v, header >> 2 );
	}

	TValue clsName;
	deserialize_string_amf3(r,context,&clsName);
	if(ttisnil(&clsName)) {
		setnilvalue(v);
		return 0;
	}

	int dynamic = 0;
	if( (header & 0x8) ) {
		// dynamic
		dynamic = 1;
	} else {
	}

	int nsealed = header >> 4;

	Table * obj = rbtH_init(r, 1, 4);
	settblvalue(v,obj);

	Traits trait = {NULL,0,0,0};
	trait.dynamic = dynamic;

	Traits * ptrait = new_traits(r, &context->traits_refs);
	if( nsealed > 0) {
		int i;
		TValue sealedName;
		TString ** name_array = RMALLOC(r, TString *, nsealed);
		for(i = 0; i < nsealed; ++i) {
			deserialize_string_amf3(r, context, &sealedName);
			if(!ttisstr(&sealedName)) {
				break;
			}
			name_array[i] = strvalue(&sealedName);

			traits_add(r, &trait, strvalue(&sealedName));

		}
		*ptrait = trait;

		TValue sealedValue;
		for(i = 0; i < nsealed; ++i) {
			decode_amf3(r,context, &sealedValue);
			setvalue(rbtH_setstr(r, obj, rbtS_gets(name_array[i])), &sealedValue);
		}
		RFREEVECTOR(r, name_array, nsealed);
	} else {
		*ptrait = trait;
	}

	TValue key,val;
	deserialize_string_amf3(r, context, &key);
	if(ttisnil(&key)) {
		return 0;
	}
	while( rbtS_len(strvalue(&key) )) {
		decode_amf3(r, context, &val);
		setvalue(rbtH_set(r, obj, &key), &val);

		deserialize_string_amf3(r, context, &key);
		if(!ttisstr(&key)) {
			break;
		}
	}


	idx_map(r, &context->obj_refs, v);

	return 0;
}

/*
 * Deserialize an array.
 * collection argument is a flag if this array is an array collection.
 */
static int deserialize_array_amf3(rabbit * r, DecodeCTX *context, int collection, TValue * v)
{
	int header = _decode_int_amf3(context);

	// Check for reference
	TValue *ref = decode_reference_amf3(r,context, &context->obj_refs, header);

	if (ref && !ttisnil(ref)) {
		// Ref found
		if (collection) {
			// Map ArrayCollection idx to ref, since
			// it points to the same list.
			idx_map(r,&context->obj_refs, ref); 
		}
		setvalue(v,ref);
		return 0;
	}

	// Create list or dict
	TValue key;
	deserialize_string_amf3(r,context,&key);
	if (ttisnil(&key)) {
		setnilvalue(v);
		return 0;
	}

	Table * t;
	int array_len = (int)(header >> 1);
	if (rbtS_len(strvalue(&key)) == 0) {
		// Regular Array
		t = rbtH_init(r,array_len,0);
	} else {
		// Associative array
		//
		// Read 1st value 
		TValue val;
		decode_amf3(r,context,&val);
		if (ttisnil(&val)) {
			setnilvalue(v);
			return 0;
		}

		t = rbtH_init(r,1,4);

		setvalue(rbtH_set(r,t,&key),&val);

		// Get rest of dict
		if (decode_dynamic_dict_amf3(r, context, t) < 0) {
			setnilvalue(v);
			return -1;
		}
	}

	settblvalue(v,t);
	// Reference must be added before children (to allow for recursion).
	idx_map(r, &context->obj_refs, v);

	// If this is an ArrayCollection,
	// we need to add another reference,
	// so there is one that
	// points to the array and one that points
	// to the collection.
	if (collection) {
		idx_map(r,&context->obj_refs, v);
	}

	// Populate list
	if (decode_dynamic_array_amf3(r, context, t, array_len) < 0) {
		setnilvalue(v);
		return 0;
	}

	return 0;
}

/* Populate an array with vals from the buffer. */
static int decode_dynamic_array_amf3(rabbit * r, DecodeCTX *context, Table * t, int array_len)
{
	// Add each item to the list
	int i;
	TValue val;
	for (i = 0; i < array_len; i++) {
		decode_amf3(r,context,&val);
		if (ttisnil(&val)) {
			return -1;
		}

		setvalue(rbtH_setnum(r,t,i),&val);
	}

	return 0;
}

/* Deserialize date. */
static int deserialize_date(rabbit * r, DecodeCTX *context, TValue * v)
{
	int header = _decode_int_amf3(context);

	// Check for reference
	TValue *date_val = decode_reference_amf3(r,context, &context->obj_refs, header);
	if (!date_val) {
		setnilvalue(v);
		return 0;
	}

	if(!ttisnil(date_val)) {
		setvalue(v,date_val);
		return 0;
	}

	double d = decode_date(context);

	setdatevalue(v,d);
	// Add reference
	idx_map(r,&context->obj_refs, v);

	return 0;
}

/* Decode a date. */
static double decode_date(DecodeCTX *context)
{
	double epoch_millisecs = _decode_double(context);

	return epoch_millisecs;
}

/* Deserialize a string. */
static int deserialize_string_amf3(rabbit * r, DecodeCTX *context, TValue * v)
{
	struct i_io * io = context->io;

	int header = 1;
	if(!io->eof(io)) {
		header = _decode_int_amf3(context);
	}

	if (header == EMPTY_STRING_TYPE) {
		const TString * es = rbtS_new(r,"");
		setstrvalue(v,es);
		return 0;
	}

	// Check for reference
	TValue *unicode_val = decode_reference_amf3(r, context, &context->string_refs, header);

	if (unicode_val && !ttisnil(unicode_val)) {
		setvalue(v,unicode_val);
		return 0;
	}

	// No reference found
	const TString * str = decode_string(r,context, header >> 1);
	if(!str) {
		setnilvalue(v);
		return -1;
	}

	setstrvalue(v,str);

	// Add to reference table
	idx_map(r,&context->string_refs, v);

	return 0;
}

/* Decode a string. */
static const TString * decode_string(rabbit * r, DecodeCTX *context, unsigned int string_size)
{
	struct i_io * io = context->io;

	const TString * ts = rbtS_init_io(r, io, string_size);
	if(!ts) {
		decode_error(context);
	}

	return ts;
}

/*
 * Checks a decoded int for the presence of a reference
 *
 * Returns TValue if obj reference was found.
 * returns PyFalse if obj reference was not found.
 * returns NULL if call failed.
 */
static TValue* decode_reference_amf3(rabbit * r, DecodeCTX *context, idxbuffer *obj_context, int val)
{
	// Check for index reference
	if ((val & REFERENCE_BIT) == 0) {
		return idx_ret_index(r, obj_context, val >> 1);
	}
	return NULL;
}

/* Decode a double to a native C double. */
static double _decode_double(DecodeCTX *context)
{
	struct i_io * io = context->io;
	char bytes[8];
	if(io->read_len(io, bytes, 8) != 8) {
		decode_error(context);
		return 0;
	}

	// Put bytes from byte array into double
	union aligned {
		double d_val;
		char c_val[8];
	} d;

	if (is_bigendian()) {
		memcpy(d.c_val, bytes, 8);
	} else {
		// Flip endianness
		d.c_val[0] = bytes[7];
		d.c_val[1] = bytes[6];
		d.c_val[2] = bytes[5];
		d.c_val[3] = bytes[4];
		d.c_val[4] = bytes[3];
		d.c_val[5] = bytes[2];
		d.c_val[6] = bytes[1];
		d.c_val[7] = bytes[0];
	}

	return d.d_val;
}

/* Decode a double to a PyFloat. */
static int decode_double(rabbit * r, DecodeCTX *context, TValue * v)
{
	double d = _decode_double(context);
	setfloatvalue(v,d);

	return 0;
}

/* Decode an int to a native C int. */
static int _decode_int_amf3(DecodeCTX *context)
{
	struct i_io * io = context->io;

	int result = 0;
	int byte_cnt = 0;
	char byte = io->read_char(io);

	// If 0x80 is set, int includes the next byte, up to 4 total bytes
	while ((byte & 0x80) && (byte_cnt < 3)) {
		result <<= 7;
		result |= byte & 0x7F;
		byte = io->read_char(io);
		if(io->error && io->error(io)) {
			decode_error(context);
			return 0;
		}
		byte_cnt++;
	}

	// shift bits in last byte
	if (byte_cnt < 3) {
		result <<= 7; // shift by 7, since the 1st bit is reserved for next byte flag
		result |= byte & 0x7F;
	} else {
		result <<= 8; // shift by 8, since no further bytes are possible and 1st bit is not used for flag.
		result |= byte & 0xff;
	}

	// Move sign bit, since we're converting 29bit->32bit
	if (result & 0x10000000) {
		result -= 0x20000000;
	}
	return result;
}

/* Decode an int to a TValue. */
static int decode_int_amf3(rabbit * r, DecodeCTX *context, TValue * v)
{
	int i = _decode_int_amf3(context); 
	setnumvalue(v,i);

	return 0;
}

/*
 *	在decode之前，需要检查传入的io是否合法
 *
 *	-- 是否实行了 read_char, error, read_len, eof
 *
 */
int check_io_valid(rabbit * r, struct i_io * io)
{
	if(	!io	||
		!io->read_char ||
		!io->read_len ||
		!io->error ||
		!io->eof
	  ) {
		kLOG(r, 0, "[Warning] %s : decode传入的io(%p)没有实现io->read_char, io->read_len, io->error 或者 io->eof\n", __FUNCTION__, io);
		return -1;
	}
	return 0;
}

/*
 * 0 - success, -1 - fail
 *
 */
int rbtAMF_decode(rabbit * r, struct i_io * in, TValue * v)
{
	if(check_io_valid(r, in) < 0) {
		return 0;
	}

	rbtStat_rt_start(r, amf3_decode);

	struct DecodeCTX context;
	decoder_init(r, &context);
	context.io = in;

	int ret = 0;

	if(!setjmp(context.err_jmp)) {
		decode_amf3(r, &context, v);
	} else {
		kLOG(r, 0, "Decode Error !\n");
		ret = -1;
	}

	decoder_dealloc(r, &context);

	rbtStat_rt_end(r, amf3_decode);

	return ret;
}

static int decode_amf3(rabbit * r, DecodeCTX * context, TValue * v)
{
	struct i_io * in = context->io;

	char byte = in->read_char(in);

	switch(byte) {
		case UNDEFINED_TYPE:
			setnilvalue(v);
			break;

		case NULL_TYPE:
			setnilvalue(v);
			break;

		case FALSE_TYPE:
			setboolvalue(v,0);
			break;

		case TRUE_TYPE:
			setboolvalue(v,1);
			break;

		case INT_TYPE:
			decode_int_amf3(r, context, v);
			break;

		case DOUBLE_TYPE:
			decode_double(r, context, v);
			break;

		case STRING_TYPE:
			deserialize_string_amf3(r, context, v);
			break;

		case DATE_TYPE:
			deserialize_date(r, context, v);
			break;

		case ARRAY_TYPE:
			deserialize_array_amf3(r, context, 0, v);
			break;

		case OBJECT_TYPE:
			decode_object(r, context,v);
			break;

		case BYTE_ARRAY_TYPE:
		case XML_TYPE:
		case XML_DOC_TYPE:
		default:
			kLOG(r, 0, "Decode Amf. Unknown Type(%x)\n", byte);
			setnilvalue(v);
			decode_error(context);
			break;
	}

	return 0;
}

