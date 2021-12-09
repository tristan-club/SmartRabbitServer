#include "config.h"
#include "gc.h"
#include "math.h"
#include "table.h"
#include "stream.h"
#include "rabbit.h"
#include "string.h"

enum TokenType {
	TT_ERROR,

	TT_NAME,

	TT_STRING,
	TT_NUMBER,

	TT_LBRACE,	// '{'
	TT_RBRACE,	// '}'

	TT_COMMA,	// ','
	TT_SEMICOLON,	// ';'

	TT_EQUAL,	// '='

	TT_EOF,		// end of file

};

static const char * token_type_str [] = {
	"TT_ERROR",
	"TT_NAME",
	"TT_STRING",
	"TT_NUMBER",
	"TT_LBRACE '{'",
	"TT_RBRACE '}'",
	"TT_COMMA ','",
	"TT_SEMICOLON ';'",
	"TT_EQUAL '='",
	"TT_EOF",
};

typedef struct Token {
	enum TokenType tt;

	union {
		const TString * ts;
		double d;
	}u;
}Token;

typedef struct Lex {
	rabbit * r;

	const char * p;
	size_t len;

	size_t cur;

	Token token;
}Lex;

static void Lex_init( Lex * lex, stream * st )
{
	setlocale(LC_CTYPE, "C");

	lex->r = NULL;

	lex->p = st->p;
	lex->len = st->size;
	lex->cur = 0;
}

#define curr(l) (l->p[l->cur])
#define nextchar(l) (l->p[l->cur+1])
#define next(l) (l->cur++)
#define is_end(l) (l->cur >= l->len)
#define is_newline(l) (curr(l) == '\r' || curr(l) == '\n')

static void skip_space( Lex * lex )
{
	while( !is_end(lex) ) {
		if(isspace(curr(lex))) {
			next(lex);
		} else {
			break;
		}
	}
}

static void skip_line( Lex * lex )
{
	while(!is_end(lex) && !is_newline(lex)) {
		next(lex);
	}
	while(!is_end(lex) && is_newline(lex)) {
		next(lex);
	}
}

static void lex_string( Lex * lex, char quotation )
{
	assert(curr(lex) == '"' || curr(lex) == '\'');

	next(lex);

	const char * p = &(curr(lex));
	int len = 0;

	while(!is_end(lex) && curr(lex) != quotation) {
		len ++;
		next(lex);
	}

	if(curr(lex) != quotation) {
		kLOG(lex->r, 0, "Config Parse Error : String Expect \". Quotation Do not match\n");
		lex->token.tt = TT_ERROR;
	}

	next(lex);

	const TString * ts = rbtS_init_len(lex->r, p, len);

	lex->token.tt = TT_STRING;
	lex->token.u.ts = ts;
}

static void lex_number( Lex * lex )
{
	assert(isdigit(curr(lex)));

	int len = 0;

	double dnum = stof(&curr(lex), lex->len - lex->cur, &len);

	lex->token.tt = TT_NUMBER;
	lex->token.u.d = dnum;

	lex->cur += len;
}

static void lex_name( Lex * lex )
{
	const char * p = &curr(lex);
	int len = 0;
	while(!is_end(lex) && !isspace(curr(lex))) {
		next(lex);
		len++;
	}

	const TString * ts = rbtS_init_len(lex->r, p, len);

	lex->token.tt = TT_NAME;
	lex->token.u.ts = ts;
}

static enum TokenType get_next_token(Lex * lex) 
{

label_begin:
	skip_space(lex);

	if( is_end(lex) ) {
		lex->token.tt = TT_EOF;
		return TT_EOF;
	}

	char quotation = '\'';

	switch( curr(lex) ) {
		case '{':
			lex->token.tt = TT_LBRACE;
			next(lex);
			break;

		case '}':
			lex->token.tt = TT_RBRACE;
			next(lex);
			break;

		case ',':
			lex->token.tt = TT_COMMA;
			next(lex);
			break;

		case ';':
			lex->token.tt = TT_SEMICOLON;
			next(lex);
			break;

		case '=':
			lex->token.tt = TT_EQUAL;
			next(lex);
			break;

		case '#':
			skip_line(lex);
			goto label_begin; break;

		case '"':
			quotation = '"';
		case '\'':
			lex_string(lex, quotation);
			break;

		case '+':
			next(lex);
			lex->token.tt = TT_NUMBER;
			lex->token.u.d = 0;
			if(is_end(lex) || !isdigit(curr(lex))) {
				break;
			}

			lex_number(lex);
			break;

		case '-':
			next(lex);
			lex->token.tt = TT_NUMBER;
			lex->token.u.d = 0;
			if(is_end(lex) || !isdigit(curr(lex))) {
				break;
			}
			lex_number(lex);
			lex->token.u.d *= -1;
			break;

		default:
			if(isdigit(curr(lex))) {
				lex_number(lex);
				break;
			}

			lex_name(lex);
			break;

	}

	return lex->token.tt;
}

static Table * read_table( Lex * lex )
{
	assert(lex->token.tt == TT_LBRACE);

	Table * t = rbtH_init(lex->r, 1, 1);

	get_next_token( lex );

	Table * tbl = rbtH_init(lex->r, 1, 1);

	while(lex->token.tt != TT_RBRACE) {

		switch(lex->token.tt) {
			case TT_ERROR:
			case TT_EOF:
				return NULL; break;

			case TT_STRING:
				setstrvalue(rbtH_setnextnum(lex->r, t), lex->token.u.ts);
				get_next_token(lex);
				break;

			case TT_NUMBER:
				setfnumvalue(rbtH_setnextnum(lex->r, t), lex->token.u.d);
				get_next_token(lex);
				break;

			case TT_LBRACE:
				tbl = read_table(lex);
				if(!tbl) {
					return NULL;
				}
				settblvalue(rbtH_setnextnum(lex->r, t), tbl);
				get_next_token(lex);
				break;

			case TT_COMMA:
				get_next_token(lex);
				break;

			default:
				kLOG(lex->r, 0, "Config Parse Warning. Array can contain 'String', 'Number', 'Array'. Unexpect Token : %s\n", token_type_str[lex->token.tt]);
				get_next_token(lex);
				break;
		}
	}

	return t;
}

static int read_config_single( Lex * lex, Table * config )
{
	const TString * ts = lex->token.u.ts;

	get_next_token(lex);

	if(lex->token.tt != TT_EQUAL) {
		return 0;
	}

	get_next_token(lex);

	Table * t;

	switch(lex->token.tt) {
		case TT_STRING:
			setstrvalue(rbtH_setstr(lex->r, config, rbtS_gets(ts)), lex->token.u.ts);
			get_next_token(lex);
			break;

		case TT_NUMBER:
			setfnumvalue(rbtH_setstr(lex->r, config, rbtS_gets(ts)), lex->token.u.d);
			get_next_token(lex);
			break;

		case TT_LBRACE:
			t = read_table(lex);
			if(!t) {
				return -1; break;
			}
			settblvalue(rbtH_setstr(lex->r, config, rbtS_gets(ts)), t);

			assert(lex->token.tt == TT_RBRACE);
			get_next_token(lex);

			break;

		default:
			kLOG(lex->r, 0, "Config Parse Error. Expect : 'String', 'Number' or 'Array'. Got : %s\n", token_type_str[lex->token.tt]);
			return -1; break;
	}


	return 0;
}

Table * read_config_from_file( rabbit * r, const char * fname )
{
	stream * st = stream_open(r, fname);
	if(!st) {
		kLOG(r, 0, "Config File : %s. Can't Open\n", fname);
		return NULL;
	}

	Lex lex1;
	Lex * lex = &lex1;
	Lex_init(lex, st);
	lex->r = r;

	Table * t = rbtH_init(r, 1, 1);

	get_next_token(lex);

	while(lex->token.tt != TT_EOF) {
		switch(lex->token.tt) {
			case TT_ERROR:
				return NULL;

			case TT_NAME:
				if(read_config_single(lex,t) < 0) {
					return NULL;
				}
				break;

			case TT_SEMICOLON:
				get_next_token(lex);
				break;

			default:
				kLOG(r, 0, "Config Warning : Unexpect Token : %s\n", token_type_str[lex->token.tt]);
				get_next_token(lex);
				break;
		}
	}

	return t;
}

