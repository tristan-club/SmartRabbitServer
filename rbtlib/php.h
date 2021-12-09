#ifndef _php_h_
#define _php_h_

#include "common.h"
#include "rabbit.h"
#include "rawbuffer.h"

#define PHP_ARRAY	'a'
#define PHP_INT		'i'
#define PHP_STRING	's'
#define PHP_DOUBLE	'd'
#define PHP_NULL	'N'
#define PHP_BOOL	'b'

#define PHP_COMMA	','
#define PHP_SEMICOLON	';'
#define PHP_COLON	':'
#define PHP_QUOTATION	'"'
#define PHP_LBRACE	'{'
#define PHP_RBRACE	'}'

int php_serialize( rabbit * r, rawbuffer * b, TValue * tv );

int php_deserialize( rabbit * r, rawbuffer * b, TValue * tv );

#endif
