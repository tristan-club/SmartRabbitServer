#include "lexical.h"
#include "gc.h"

#include "rabbit.h"

#include "string.h"

#define pos(l,n) (*cast(char*,(l)->file->p + n))
#define curr(l) pos(l,l->curr)
#define next(l) ((l)->curr++)
#define is_end(l) ((l)->curr >= (l)->file->size)

#define is_newline(c) ( (c) == '\r' || (c) == '\n' )

const char * lex_key_word[] = {
	"nil",			// LEX_NIL or LEX_NULL
	"null",			// LEX_NIL or LEX_NULL
	"false",		// LEX_FALSE
	"true",			// LEX_TRUE
	"local",		// LEX_LOCAL
	"global",		// LEX_GLOBAL
	"function",		// LEX_FUNCTION
	"return",		// LEX_RETURN
	"if",			// LEX_IF
	"then",			// LEX_THEN
	"else",			// LEX_ELSE
	"elseif",		// LEX_ELSEIF
	"while",		// LEX_WHILE
	"do",			// LEX_DO
	"for",			// LEX_FOR
	"foreach",		// LEX_FOREACH
	"as",			// LEX_AS
	"break",		// LEX_BREAK
	"continue",		// LEX_CONTINUE
	"include",		// LEX_INCLUDE
	"end"			// LEX_END
};

const char * lex_lex_str[] = {
	"error",
	"eof",
	"eol",

	"nil",
	"null",
	"false",
	"true",
	"local",
	"global",
	"function",
	"return",
	"if",
	"then",
	"else",
	"elseif",
	"while",
	"do",
	"for",
	"foreach",
	"as",
	"break",
	"continue",
	"include 'xxx'",
	"end",

	"inc '++'",
	"des '--'",

	"comma ','",
	"semicolon ';'",
	"colon ':'",
	"quotation '\"'",
	"dot '.' ",	
	"left brace '{'",
	"right brace '}'",
	"left bracket '['",
	"right bracket ']'",
	"left parentheses '('",
	"right parentheses ')'",

	"name",
	"number",
	"string",
	"assignment '='",

	"less than '<'",
	"less than or equal '<='",
	"great than '>'",
	"great than or equal '>='",
	"equal '=='",
	"not equal '!='",

	"plus '+'",
	"minus '-'",
	"multi '*'",
	"div '/'",

	"not '!'",
	"negtive '-'",
	"positive '+'",

	"remainder '%'",

	"Not A Type"
};

static int g_nLex = 0;
static int g_mLex = 0;

int script_lex_debug_count()
{
	return g_nLex;
}

int script_lex_debug_mem()
{
	return g_mLex;
}

lexical * script_lex_init( Script * S, stream * s )
{
	rabbit * r = S->r;

	setlocale(LC_CTYPE,"C");	// for isspace

	lexical * lex = RMALLOC(S->r, lexical, 1);

	r->obj++;
	g_nLex++;
	g_mLex += sizeof(lexical);

	lex->file = s;
	lex->curr = 0;

	if(!s) {
		kLOG(S->r, 0,"File does not exist.\n");
		exit(0);
	}

	lex->b = buffer_init(S->r);

	lex->S = S;

	lex->fs = NULL;

	lex->filename = s->filename;
	lex->line = 0;

	const char * pdir = strrchr(rbtS_gets(lex->filename), '/');
	if(pdir) {
		lex->directory = rbtS_init_len(S->r, rbtS_gets(lex->filename), pdir - rbtS_gets(lex->filename));
	} else {
		lex->directory = rbtS_new(S->r, "");
	}

	lex->current_global = NULL;

	return lex;
}

void script_lex_free(rabbit * r, struct lexical * lex)
{
	r->obj--;
	g_nLex--;
	g_mLex -= sizeof(struct lexical);

	buffer_free(lex->b);
	RFREE(r, lex);
}

static void skip_line( lexical * lex )
{
	while(!is_end(lex) && !is_newline(curr(lex))) {
	       	next(lex);
	}

	/*
	while(!is_end(lex) && is_newline(curr(lex))) {
		next(lex);
		lex->line++;
	}*/
	while(!is_end(lex)) {
		char c = curr(lex);
		if(c == '\n') {
			next(lex);
			lex->line++;
			continue;
		}
		if(c == '\r') {
			next(lex);
			continue;
		}
		break;
	}
}

static void skip_space( lexical * lex )
{
	while(lex->curr < lex->file->size) {
		if(!isspace(curr(lex))) {
			break;
		}
		if(curr(lex) == '\r' || curr(lex) == '\n') {
			skip_line(lex);
		} else {
			next(lex);
		}
	}
}

static void save( lexical * lex, char c )
{
	Buffer * b = lex->b;
	if(b->used >= b->size) {
		buffer_prepare_append(lex->S->r, b, 16);
	}
	b->p[b->used++] = c;
}

static const TString * lex_string( lexical * lex )
{
	char quto = curr(lex);
	next(lex);

	if(quto != '\'' && quto != '"') return NULL;
	lex->b->used = 0;

	while( !is_end(lex) && curr(lex) != quto ) {
		switch( curr(lex) ) {
			case '\\':
				next(lex);
				if(is_end(lex)) {
					kLOG(lex->S->r, 0, "Unfinished String\n");
					break;
				}
				switch( curr(lex) ) {
					case 'a': save(lex,'\a');break;
					case 'b': save(lex,'\b');break;
					case 'f': save(lex,'\f');break;
					case 'n': save(lex,'\n');break;
					case 'r': save(lex,'\r');break;
					case 't': save(lex,'\t');break;
					case 'v': save(lex,'\v');break;
					case '\\': save(lex,'\\');break;
					case '\'': save(lex,'\'');break;
					case '"': save(lex,'"');break;
					case '?': save(lex,'\?');break;
					default:
						  break;
				}
				next(lex);
				break;
			default:
				save(lex,curr(lex));
				next(lex);
				break;
		}
	}

	if(is_end(lex)) {
		kLOG(lex->S->r, 0, "Unfinished String\n");
		return NULL;
	}

	next(lex);	// skip "
	const TString * ts = rbtS_init_len(lex->S->r,lex->b->p,lex->b->used);

	return ts;
}

static int lex_uint( lexical * lex, int * len )
{
	*len = 0;
	int i = 0;
	while(!is_end(lex) && isdigit(curr(lex))) {
		i = i * 10 + curr(lex) - '0';
		next(lex);
		(*len)++;
	}
	return i;
}

static double lex_number( lexical * lex )
{
	double d = 0;

	int len;
	
	int n = lex_uint(lex, &len);
	int f = 0;
	len = 0;

	if(curr(lex) == '.') {
		next(lex);
		f = lex_uint(lex, &len);
	}
	d = f;
	while(len > 0) {
		d /= 10;
		len--;
	}

	return d + n;
}

Token lex_name_key_word( lexical * lex )
{
	const char * p = &curr(lex);
	int len = 0;
	while(!is_end(lex) && (isalnum(curr(lex)) || curr(lex) == '_')) {
		len++;
		next(lex);
	}
	int i;
	for(i = LEX_NIL; i <= LEX_END; i++) {
		if(len == strlen(lex_key_word[i-LEX_NIL]) && strncmp(p,lex_key_word[i-LEX_NIL],len) == 0) {
			lex->token.tt = i;
			if(lex->token.tt == LEX_NULL) {
				lex->token.tt = LEX_NIL;
			}
			return lex->token;
		}
	}

	lex->token.tt = LEX_NAME;

	const TString * ts = rbtS_init_len(lex->S->r,p,len);
	setstrvalue(&lex->token.tv,ts);

	return lex->token;
}

Token lex_next_token( lexical * lex )
{
	skip_space( lex );
	Token * token = &lex->token;
	if(lex->curr >= lex->file->size) {
		token->tt = LEX_EOF;
		return *token;
	}

	const TString * ts;
	switch ( curr(lex) ) {
		case ',':
			token->tt = LEX_COMMA;
			next(lex);
			break;

		case ';':
			token->tt = LEX_SEMICOLON;
			next(lex);
			break;

		case ':':
			token->tt = LEX_COLON;
			next(lex);
			break;

		case '#':
			next(lex);
			skip_line(lex);
			return lex_next_token(lex);

		case '^':
			token->tt = LEX_TIP;
			next(lex);
			break;

		case '.':
			token->tt = LEX_DOT;
			next(lex);
			if(!is_end(lex)) {
				if(curr(lex) == '.') {
					token->tt = LEX_CONCATENATE;
					next(lex);
				}
			}

			break;

		case '+':
			token->tt = LEX_PLUS;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '+') {
					token->tt = LEX_INC;
					next(lex);
				}
				if(isdigit(curr(lex))) {
					token->tt = LEX_NUMBER;
					setfloatvalue(&token->tv, lex_number(lex));
				}
			}
			break;

		case '-':
			token->tt = LEX_MINUS;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '-') {
					token->tt = LEX_DES;
					next(lex);
				}
				if(isdigit(curr(lex))) {
					token->tt = LEX_NUMBER;
					setfloatvalue(&token->tv,-1 * lex_number(lex));
				}
			}
			break;

		case '*':
			token->tt = LEX_MULTI;
			next(lex);
			break;

		case '/':
			token->tt = LEX_DIV;
			next(lex);
			if(!is_end(lex)) {
				if(curr(lex) == '/') {
					skip_line(lex);
					return lex_next_token(lex);
				}
			}
			break;

		case '\'':
		case '"':
			token->tt = LEX_STRING;
			if((ts = lex_string(lex)) == NULL) {
				token->tt = LEX_ERROR;
				return *token;
			}
			setstrvalue(&token->tv,ts);
			break;
		case '=':
			token->tt = LEX_ASSIGN;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '=') {
					token->tt = LEX_EQ;
					next(lex);
				}
			}
			break;
		case '<':
			token->tt = LEX_LT;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '=') {
					token->tt = LEX_LE;
					next(lex);
				}
			}
			break;
		case '>':
			token->tt = LEX_GT;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '=') {
					token->tt = LEX_GE;
					next(lex);
				}
			}
			break;
		case '!':
			token->tt = LEX_NOT;
			next(lex);

			if(!is_end(lex)) {
				if(curr(lex) == '=') {
					token->tt = LEX_NE;
					next(lex);
				}
			}
			break;

		case '(':
			token->tt = LEX_LPARENTHESES;
			next(lex);
			break;

		case ')':
			token->tt = LEX_RPARENTHESES;
			next(lex);
			break;

		case '[':
			token->tt = LEX_LBRACKET;
			next(lex);
			break;

		case ']':
			token->tt = LEX_RBRACKET;
			next(lex);
			break;

		case '{':
			token->tt = LEX_LBRACE;
			next(lex);
			break;

		case '}':
			token->tt = LEX_RBRACE;
			next(lex);
			break;

		case '%':
			token->tt = LEX_REMAINDER;
			next(lex);
			break;

		case '&':
			token->tt = LEX_AND;
			next(lex);
			if(!is_end(lex)) {
				if(curr(lex) == '&') {
					next(lex);
				}
			} else {
				token->tt = LEX_ERROR;
			}
			break;

		case '$':
			token->tt = LEX_DOLLAR;
			next(lex);
			break;

		case '|':
			token->tt = LEX_OR;
			next(lex);
			if(!is_end(lex)) {
				if(curr(lex) == '|') {
					next(lex);
				}
			} else {
				token->tt = LEX_ERROR;
			}
			break;

		default:
			if(isdigit(curr(lex))) {
				token->tt = LEX_NUMBER;
				setfloatvalue(&token->tv, lex_number(lex));
				break;
			}
			return lex_name_key_word(lex);
	}

	return *token;
}

