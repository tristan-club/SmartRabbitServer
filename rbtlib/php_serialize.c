#include "php.h"
#include "math.h"
#include "string.h"
#include "table.h"

static int encode_write_byte( rabbit * r, rawbuffer * buf, char c )
{
	char b[1];
	b[0] = c;

	return buffer_write(r, buf, b, 1);
}

static int encode_nil( rabbit * r, rawbuffer * buf )
{
	return encode_write_byte(r, buf, PHP_NULL);
}

static int encode_int( rabbit * r, rawbuffer * buf, int i )
{
	encode_write_byte(r, buf, PHP_INT);
	encode_write_byte(r, buf, PHP_COLON);	// i:

	char * p = itos(i);

	return buffer_write(r, buf, p, strlen(p));
}

static int encode_float( rabbit * r, rawbuffer * buf, double d )
{
	encode_write_byte(r, buf, PHP_DOUBLE);
	encode_write_byte(r, buf, PHP_COLON);

	char * p = ftos(d);

	return buffer_write(r, buf, p, strlen(p));
}

static int encode_string( rabbit * r, rawbuffer * buf, TString * ts )
{
	encode_write_byte(r, buf, PHP_STRING);
	encode_write_byte(r, buf, PHP_COLON);

	char * p = utos(rbtS_len(ts));
	buffer_write(r, buf, p, strlen(p));

	encode_write_byte(r, buf, PHP_COLON);

	encode_write_byte(r, buf, PHP_QUOTATION);

	buffer_write(r, buf, cast(char *, rbtS_gets(ts)), rbtS_len(ts));

	encode_write_byte(r, buf, PHP_QUOTATION);

	return 0;
}

static int encode_bool( rabbit * r, rawbuffer * buf, int b )
{
	encode_write_byte(r, buf, PHP_BOOL);
	encode_write_byte(r, buf, PHP_COLON);

	if(b) {
		buffer_write(r, buf, "t", 1);
	} else {
		buffer_write(r, buf, "f", 1);
	}

	return 0;
}

static int encode_table( rabbit * r, rawbuffer * buf, Table * t )
{
	encode_write_byte(r, buf, PHP_ARRAY);
	encode_write_byte(r, buf, PHP_COLON);

	int count = rbtH_count(r, t);
	char * p = utos(count);
	buffer_write(r, buf, p, strlen(p));

	encode_write_byte(r, buf, PHP_COLON);
	encode_write_byte(r, buf, PHP_LBRACE);

	int idx = -1;
	TValue key, val;
	while(1) {
		idx = rbtH_next(r, t, idx, &key, &val);
		if(idx < 0) {
			break;
		}

		if(!ttisstr(&key) && !ttisnum(&key)) {
			continue;
		}

		if(php_serialize( r, buf, &key ) < 0) {
			return -1;
		}

		encode_write_byte( r, buf, PHP_SEMICOLON );

		if(php_serialize( r, buf, &val ) < 0) {
			return -1;
		}

		if(!ttistbl(&val)) {
			encode_write_byte( r, buf, PHP_SEMICOLON );
		}
	}

	encode_write_byte(r, buf, PHP_RBRACE);

	return 0;
}

int php_serialize( rabbit * r, rawbuffer * buf, TValue * tv )
{
	if(!tv) {
		return -1;
	}

	switch( ttype(tv) ) {
		case TNIL:
			return encode_nil( r, buf );

		case TNUMBER:
			return encode_int( r, buf, numvalue(tv) );

		case TFLOAT:
			return encode_float(r, buf, fnumvalue(tv));

		case TSTRING:
			return encode_string(r, buf, strvalue(tv));

		case TTABLE:
			return encode_table(r, buf, tblvalue(tv));

		case TBOOL:
			return encode_bool( r, buf, bvalue(tv));

		default:
			kLOG(r, 0, "PHP Serialize support : TNIL, TNUMBER, TFLOAT, TSTRING, TTABLE\n");
			return encode_nil(r, buf);
	}

	return -1;
}

