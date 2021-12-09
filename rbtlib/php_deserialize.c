#include "inclib.h"
#include "rawbuffer.h"
#include "php.h"

static int decode_read_byte( rawbuffer * buf )
{
	char * p = buffer_read(buf, 1);
	if(p) {
		return *p;
	}
	return 0;
}

static int decode_null( rabbit * r, rawbuffer * buf, TValue * tv )
{
	setnilvalue(tv);

	return 0;
}

static int decode_int( rabbit * r, rawbuffer * buf, TValue * tv )
{
	int c = decode_read_byte( buf );	// skip ':'
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 'i'\n");
		return -1;
	}

	int len;
	int i = stoi(buf->buf + buf->pos, buf->len - buf->pos, &len);
	buf->pos += len;

	setnumvalue(tv, i);

	return 0;
}

static int decode_double( rabbit * r, rawbuffer * buf, TValue * tv )
{
	int c = decode_read_byte( buf );	// skip ':'
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 'd'\n");
		return -1;
	}

	int len;
	double d = stof(buf->buf + buf->pos, buf->len - buf->pos, &len);
	buf->pos += len;

	setfloatvalue(tv, d);

	return 0;
}

static int decode_string( rabbit * r, rawbuffer * buf, TValue * tv )
{
	int c = decode_read_byte( buf );	// skip ":"
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 's'\n");
		return -1;
	}

	int len;
	int i = stoi(buf->buf + buf->pos, buf->len - buf->pos, &len);
	buf->pos += len;

	c = decode_read_byte( buf );
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 's:len'\n");
		return -1;
	}

	c = decode_read_byte( buf );
	if(c != PHP_QUOTATION) {
		kLOG(r, 0, "PHP Deserialize : Expect '\"' in Decode String\n");
		return -1;
	}

	const TString * ts = rbtS_init_len(r, buf->buf + buf->pos, i);
	buf->pos += i;

	c = decode_read_byte( buf );
	if(c != PHP_QUOTATION) {
		kLOG(r, 0, "PHP Deserialize : Expect '\"' in Decode String\n");
		return -1;
	}

	setstrvalue(tv, ts);
	return 0;
}

static int decode_bool( rabbit * r, rawbuffer * buf, TValue * tv )
{
	int c = decode_read_byte( buf );
	if(c != PHP_COLON ) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 'b' \n");
		return -1;
	}

	c = decode_read_byte( buf );

	if(c == 't') {
		setboolvalue(tv, 1);
	} else {
		setboolvalue(tv, 0);
	}

	return 0;
}

static int decode_array( rabbit * r, rawbuffer * buf, TValue * tv )
{
	int c = decode_read_byte( buf );	// skip ":"
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 'a'\n");
		return -1;
	}

	int len;
	int count = stoi(buf->buf + buf->pos, buf->len - buf->pos, &len);
	buf->pos += len;

	c = decode_read_byte( buf );
	if(c != PHP_COLON) {
		kLOG(r, 0, "PHP Deserialize : Expect ':' After 'a:count'\n");
		return -1;
	}

	c = decode_read_byte( buf );
	if(c != PHP_LBRACE) {
		kLOG(r, 0, "PHP Deserialize : Expect '{' in Decode Array\n");
		return -1;
	}

	Table * t = rbtH_init(r, 1, 4);

	int i;
	TValue key, val;
	for(i = 0; i < count; ++i) {
		if(php_deserialize(r, buf, &key) < 0) {
			return -1;
		}
		c = decode_read_byte( buf );
		if(c != PHP_SEMICOLON) {
			kLOG(r, 0, "PHP Deserialize : Expect ';' Between 'key' and 'value' in Decode Array\n");
			return -1;
		}
		if(php_deserialize(r, buf, &val) < 0) {
			return -1;
		}

		c = decode_read_byte( buf );
		if(c != PHP_SEMICOLON) {
			buf->pos--;
		}

		setvalue(rbtH_set(r, t, &key), &val);
	}

	c = decode_read_byte( buf );
	if(c != PHP_RBRACE) {
		kLOG(r, 0, "PHP Deserialize : Expect '}' when Array close\n");
		return -1;
	}

	settblvalue(tv, t);

	return 0;
}

int php_deserialize( rabbit * r, rawbuffer * buf, TValue * tv )
{
	if(!buf || !tv) {
		return -1;
	}
	setnilvalue(tv);

	int c = decode_read_byte( buf );

	switch( c ) {
		case PHP_NULL:
	//		kLOG(r, 0,"php decode nil\n");
			return decode_null( r, buf, tv );

		case PHP_INT:
	//		kLOG(r, 0,"php decode int\n");
			return decode_int( r, buf, tv );

		case PHP_DOUBLE:
	//		kLOG(r, 0,"php decode double\n");
			return decode_double(r, buf, tv);

		case PHP_STRING:
	//		kLOG(r, 0,"php decode string\n");
			return decode_string(r, buf, tv);

		case PHP_ARRAY:
	//		kLOG(r, 0,"php decode array\n");
			return decode_array(r, buf, tv);

		case PHP_BOOL:
			return decode_bool(r, buf, tv);

		default:
			kLOG(r, 0, "php decode unknow:%c\n",c);
			break;
	}

	return -1;
}

