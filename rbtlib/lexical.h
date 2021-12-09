#ifndef _lexical_h_
#define _lexical_h_

#include "script.h"

#include "object.h"
#include "mem.h"
#include "string.h"
#include "buffer.h"
#include "stream.h"

#include <locale.h>
#include <ctype.h>


typedef enum {
	LEX_ERROR,      // lexical parse error

	LEX_EOF,        // end of file
	LEX_EOL,        // end of line
	
	// LEX_KEY_WORD
		LEX_NIL,        // key word: nil
		LEX_NULL,	// key word: null

		LEX_FALSE,	// key word: false
		LEX_TRUE,	// key word: true

		LEX_LOCAL,      // key word: local
		LEX_GLOBAL,     // key word: global

		LEX_FUNCTION,   // key word: function
		LEX_RETURN,	// key word: return
		LEX_IF,         // key word: if	
		LEX_THEN,	// key word: then
		LEX_ELSE,	// key word: else
		LEX_ELSEIF,	// key word: elseif
		LEX_WHILE,      // key word: while
		LEX_DO,		// key word: do	
		LEX_FOR,        // key word: for
		LEX_FOREACH,    // key word: foreach
		LEX_AS,         // key word: as	
		LEX_BREAK,	// key word: break
		LEX_CONTINUE,	// key word: continue

		LEX_INCLUDE,	// key word: include

		LEX_END,        // key word: end

	LEX_INC,        // ++
	LEX_DES,        // --

	LEX_COMMA,      // comma -- ,
	LEX_SEMICOLON,  // semicolon -- ;

	LEX_COLON,      // colon -- :

	LEX_QUOTATION,  // quotation -- "

	LEX_DOT,        // dot -- .

	LEX_LBRACE,     // brace -- {
	LEX_RBRACE,     // brace -- }
	LEX_LBRACKET,   // bracket -- [	
	LEX_RBRACKET,   // bracket -- ]
	LEX_LPARENTHESES,       // parentheses -- (
	LEX_RPARENTHESES,       // parentheses -- )

	LEX_NAME,       // 函数名/变量名等 -- 以字母,下划线开头,由字母,下划线,数字组成

	LEX_NUMBER,     // 数字

	LEX_STRING,     // 字符串

	LEX_ASSIGN,     // assignment -- =

	LEX_LT,         // less than -- <
	LEX_LE,         // less equal -- <=
	LEX_GT,         // great than -- >
	LEX_GE,         // great equal -- >=
	LEX_EQ,         // equal -- ==
	LEX_NE,         // not equal -- !=

	LEX_PLUS,       // binary operator -- +
	LEX_MINUS,      // binary operator -- -
	LEX_MULTI,      // binary operator -- *
	LEX_DIV,        // binary operator -- /

	LEX_NOT,        // unary operator -- !
	LEX_NEGTIVE,   // unary operator -- -
	LEX_POSITIVE,   // unary operator -- +

	LEX_REMAINDER,	// binary operator -- %

	LEX_AND,	// &&
	LEX_OR,		// ||

	LEX_CONCATENATE, // binary operator -- .. concatenate strings

	LEX_TIP,	// unary operator -- ^ caculate the lenght of

	LEX_DOLLAR,	// unary operator -- $ 根据字符串取变量

	LEX_TYPE_END,	//

}LEX_TYPE;

extern const char * lex_key_word[];

extern const char * lex_lex_str[];

typedef struct Token {
	int tt;
	TValue tv;
}Token;

typedef struct lexical {
	stream * file;

	size_t curr;

	Token token;

	Buffer * b;

	Script * S;

	const TString * filename;
	const TString * directory;
	size_t line;

	Table * current_global;

	struct FuncState * fs;
}lexical;

lexical * script_lex_init( Script * S, stream * s );

void script_lex_free(rabbit * r, struct lexical * lex);

Token lex_next_token( lexical * lex );

int script_lex_debug_count();
int script_lex_debug_mem();

#endif
