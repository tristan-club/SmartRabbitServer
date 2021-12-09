#include "parser.h"

#include "lexical.h"
#include "proto.h"
#include "opcode.h"
#include "code.h"

#include "object.h"

#include "table.h"

#include "rabbit.h"

#include "common.h"
#include "gc.h"
#include "script.h"
#include "script_load.h"

#define BLOCK_MAX_BREAK_CONTINUE	256
struct Block {
	int breaks[BLOCK_MAX_BREAK_CONTINUE];
	int bused;

	int continues[BLOCK_MAX_BREAK_CONTINUE];
	int cused;
};

#define BLOCK_TYPE_MAIN		0
#define BLOCK_TYPE_IF		1
#define BLOCK_TYPE_FOR		2
#define BLOCK_TYPE_WHILE	3
#define BLOCK_TYPE_FOREACEH	4
#define BLOCK_TYPE_FUNCTION	5

static int addk( FuncState * fs, TValue * key, TValue * val );
static int block( lexical * lex, struct Block *, int block_type, int use_brace );
static int expr( lexical * lex, expression * v );
static int explist( lexical * lex );
static int new_local_var( lexical * lex );

#if 1
#define assert_freereg(lex) {	\
	if(lex->fs->freereg != lex->fs->nlocvar) {	\
		ParseWarn(lex, "Freereg (%d) != nLocvars (%d)\n", lex->fs->freereg, lex->fs->nlocvar);	\
		assert(0);	\
	}	\
}
#else 
#define assert_freereg(lex)
#endif


void ParseError( lexical * lex, const char * format,...) 
{
	static char buffer[256];
	memset(buffer,0,256);

	va_list ap;
	va_start(ap,format);
	int i = snprintf(buffer,255,"ERROR\t%s(%zu):",rbtS_gets(lex->filename),lex->line+1);
	vsnprintf(buffer+i,255-i,format,ap);
	va_end(ap);

	die( lex->S, buffer );
}

static void ParseWarn( lexical * lex, const char * format, ... )
{
	static char buffer[256];
	memset(buffer,0,256);

	va_list ap;
	va_start(ap,format);
	int i = snprintf(buffer,255,"[Script]WARNING\t:%s(%zu):",rbtS_gets(lex->filename),lex->line+1);
	vsnprintf(&buffer[i],255-i,format,ap);
	va_end(ap);

//	fprintf(stderr, buffer);
	kLOG(lex->fs->r, 0, "%s", buffer);
}

static int test( lexical * lex, int t )
{
	if(lex->token.tt == t) {
		return 1;
	}
	return 0;
}

static int testnext( lexical * lex , int t )
{
	if(test(lex,t)){
		lex_next_token(lex);
		return 1;
	}
	return 0;
}

static int check( lexical * lex, int t )
{
	if(!test(lex,t)) {
		ParseError(lex,"Expect :%s, Get:%s.\n",lex_lex_str[t],lex_lex_str[lex->token.tt]);
	}
	return 1;
}

static int checknext( lexical * lex, int t )
{
	if(testnext(lex, t) == 0) {
		ParseError(lex,"Expect :%s, Get:%s.\n",lex_lex_str[t],lex_lex_str[lex->token.tt]);
	}
	return 1;
}

static int open_func( rabbit * r, FuncState * fs, lexical * lex )
{
	fs->p = rbtP_proto( r );
	fs->p->env = lex->current_global;

	fs->lex = lex;
	fs->r = r;
	fs->parent = lex->fs;
	lex->fs = fs;

	fs->nlocvar = 0;
	fs->pc = 0;
	fs->nk = 0;
	fs->np = 0;

	fs->freereg = 0;
	fs->nuv = 0;

	fs->uv_level = -1;

	FuncState * f = lex->fs->parent;

	if(!f) {
		return -1;
	}

	if(f->np >= f->p->sizep) {
		size_t new_size = (f->np+1)*2;
		f->p->p = RREALLOC(r, Proto*, f->p->p, f->p->sizep, new_size);
		f->p->sizep = new_size;
	}
	f->p->p[f->np] = fs->p;
	fs->p->parent = f->p;

	return f->np++;
}

static void close_func( FuncState * fs )
{
	lexical * lex = fs->lex;
	lex->fs = fs->parent;

	code_ret(fs,0,0);

	fs->p->locvars = RREALLOC(fs->r, LocVar, fs->p->locvars, fs->p->sizelocvars, fs->nlocvar);
	fs->p->sizelocvars = fs->nlocvar;

	fs->p->i = RREALLOC(fs->r, Instruction, fs->p->i, fs->p->sizei, fs->pc);
	fs->p->line = RREALLOC(fs->r, int, fs->p->line, fs->p->sizei, fs->pc);
	fs->p->sizei = fs->pc;

	fs->p->k = RREALLOC(fs->r, TValue, fs->p->k, fs->p->sizek, fs->nk);
	fs->p->sizek = fs->nk;

	fs->p->p = RREALLOC(fs->r, Proto*, fs->p->p, fs->p->sizep, fs->np);
	fs->p->sizep = fs->np;
}


static void add_closure( Script * S, Closure * cl )
{
	if(S->closureused >= S->closuresize) {
		S->files = RREALLOC(S->r, Closure *, S->files, S->closuresize, S->closuresize + 4);
		S->closuresize += 4;
	}
	S->files[S->closureused++] = cl;
}

int script_parse( Script * S, stream * file, Table * global )
{
	FuncState fs;
	lexical * lex = script_lex_init(S, file);

	if(!global) {
		global = S->global;
	}

	rbtC_stable(cast(GCObject *, global));
	rbtC_stable(cast(GCObject *, file->filename));

	lex->current_global = global;

	open_func( S->r, &fs, lex );

	fs.p->file = fs.p->fname = cast(TString *, file->filename);

	lex_next_token( lex );

	block( lex, NULL, BLOCK_TYPE_MAIN, 0 );
	close_func( &fs );
	
	Closure * cl = rbtScript_closure( S );
	cl->isC = 0;
	cl->u.p = fs.p;
	cl->pc = 0;

	add_closure(S, cl);
	
	if(S->generate_zro) {
		static char buf[256];
		memset(buf, 0, sizeof(buf));
		const char * path = rbtS_gets(file->filename);
		int len = strlen(path);
		memcpy(buf, path, len - 4);
		buf[len-4] = '.';
		buf[len-3] = 'z';
		buf[len-2] = 'r';
		buf[len-1] = 'o';
		buf[len] = 0;
		script_save(S, fs.p, buf);

		memset(buf, 0, sizeof(buf));
		snprintf(buf, 255, "%s.script", path);
		rbtD_proto_to_file(fs.p, buf);
	}

	script_lex_free(S->r, lex);

	return 0;
}

enum BinaryOpr {
	BOP_ADD,
	BOP_MINUS,
	BOP_MULTI,
	BOP_DIV,
	BOP_GT,
	BOP_GE,
	BOP_LT,
	BOP_LE,
	BOP_EQ,
	BOP_NE,
	BOP_REMAINDER,
	BOP_AND,
	BOP_OR,
	BOP_CONCATENATE,
	NOT_BOP,
};

static const int biop_priority[] = {
	3,3,	// + , - 
	4,4,	// * , / 
	2,2,2,2,2,2,	// > , >= , < , <= , == , !=
	4,	// %
	1, 1,	// &&, ||
	6,	// ..
	0
};

static int get_binary_oprator( lexical * lex )
{
	if(lex->token.tt == LEX_PLUS) {
		return BOP_ADD;
	}
	if(lex->token.tt == LEX_MINUS) {
		return BOP_MINUS;
	}
	if(lex->token.tt == LEX_MULTI) {
		return BOP_MULTI;
	}
	if(lex->token.tt == LEX_DIV) {
		return BOP_DIV;
	}
	if(lex->token.tt == LEX_GT) {
		return BOP_GT;
	}
	if(lex->token.tt == LEX_GE) {
		return BOP_GE;
	}
	if(lex->token.tt == LEX_LT) {
		return BOP_LT;
	}
	if(lex->token.tt == LEX_LE) {
		return BOP_LE;
	}
	if(lex->token.tt == LEX_EQ) {
		return BOP_EQ;
	}
	if(lex->token.tt == LEX_NE) {
		return BOP_NE;
	}
	if(lex->token.tt == LEX_REMAINDER) {
		return BOP_REMAINDER;
	}
	if(lex->token.tt == LEX_AND) {
		return BOP_AND;
	}
	if(lex->token.tt == LEX_OR) {
		return BOP_OR;
	}
	if(lex->token.tt == LEX_CONCATENATE) {
		return BOP_CONCATENATE;
	}

	return NOT_BOP;
}

enum UnaryOpr {
	UOP_NEGTIVE,
	UOP_OPSITIVE,
	UOP_NOT,
	UOP_TIP,
	UOP_VALUE,
	NOT_UOP,
};

#define UNARYOPR_PRIORITY	8

static int get_unary_oprator( lexical * lex )
{
	if(lex->token.tt == '-') {
		return UOP_NEGTIVE;
	}
	if(lex->token.tt == '+') {
		return UOP_OPSITIVE;
	}
	if(lex->token.tt == LEX_NOT) {
		return UOP_NOT;
	}
	if(lex->token.tt == LEX_TIP) {
		return UOP_TIP;
	}
	if(lex->token.tt == LEX_DOLLAR) {
		return UOP_VALUE;
	}

	return NOT_UOP;
}

static int not_uop( lexical * lex, expression * v )
{
	code_exp2nextreg(lex->fs,v);
	code_not( lex->fs, v );

	return 0;
}

static int length_uop( lexical * lex, expression * v )
{
	code_exp2nextreg(lex->fs,v);
	code_length(lex->fs, v);

	return 0;
}

static int value_uop( lexical * lex, expression * v )
{
	code_exp2nextreg(lex->fs, v);
	code_value(lex->fs, v);
	return 0;
}

static int prefix( lexical * lex, expression * v, int uop )
{
	if(uop == UOP_NOT) {
		not_uop(lex,v);
	}
	if(uop == UOP_TIP) {
		length_uop(lex,v);
	}
	if(uop == UOP_VALUE) {
		value_uop(lex,v);
	}
	return 0;
}

static int infix_aux_get_reg( lexical * lex, expression * v, expression * v2 )
{
	int reg = 0;

#define IS(v) EXP2REG_DONTMOVE(v->tt)

	if(IS(v) && IS(v2)) {
		reg = lex->fs->freereg++;
	} else
	if(IS(v)) {
		reg = v2->i;
		lex->fs->freereg = reg + 1;
	} else
	if(IS(v2)) {
		reg = v->i;
		lex->fs->freereg = reg + 1;
	} else {
		reg = min(v->i, v2->i);
		lex->fs->freereg = reg + 1;
	}
#undef IS
	return reg;
}

static int infix( lexical * lex, expression * v, expression * v2, int bop, int pc )
{
	int reg = 0;
	if(bop == BOP_AND || bop == BOP_OR) {
		code_exp2nextreg(lex->fs, v2);
		reg = min(v->i, v2->i);
		lex->fs->freereg = reg + 1;
	} else {
		code_exp2reg(lex->fs, v2);
		reg = infix_aux_get_reg(lex, v, v2);
	}

	switch( bop ) {
		case BOP_ADD:
			reg = code_add(lex->fs, v, v2, reg);
			break;
		case BOP_MINUS:
			reg = code_minus(lex->fs, v, v2, reg);
			break;
		case BOP_MULTI:
			reg = code_multi(lex->fs, v, v2, reg);
			break;
		case BOP_DIV:
			reg = code_div(lex->fs, v, v2, reg);
			break;
		case BOP_GT:
			reg = code_gt(lex->fs, v, v2, reg);
			break;
		case BOP_GE:
			reg = code_ge(lex->fs, v, v2, reg);
			break;
		case BOP_LT:
			reg = code_lt(lex->fs, v, v2, reg);
			break;
		case BOP_LE:
			reg = code_le(lex->fs, v, v2, reg);
			break;
		case BOP_EQ:
			reg = code_eq(lex->fs, v, v2, reg);
			break;
		case BOP_NE:
			reg = code_ne(lex->fs, v, v2, reg);
			break;
		case BOP_REMAINDER:
			reg = code_remainder(lex->fs, v, v2, reg);
			break;
		case BOP_AND:
			reg = code_and(lex->fs, v, v2, pc, reg);
			break;
		case BOP_OR:
			reg = code_or(lex->fs, v, v2, pc, reg);
			break;
		case BOP_CONCATENATE:
			reg = code_concatenate(lex->fs, v, v2, reg);
			break;
		default:
			break;
	}

	v->i = reg;
	v->tt = EXP_NORELOCATE;
//	lex->fs->freereg = reg + 1;

	return 0;
}

static int search_var( FuncState * fs, const TString * name )
{
	int i;
	for(i = 0; i < fs->nlocvar; ++i) {
		if(fs->p->locvars[i].name == name) {
			return i;
		}
	}

	return -1;
}

static int indexupvalue( FuncState * fs, const TString * name, expression * v)
{
	int i;
	for(i = 0; i < fs->nuv; ++i) {
		if(fs->upvalues[i].k == v->tt && fs->upvalues[i].info == v->i) {
			return i;
		}
	}

	if(fs->nuv >= MAXUPVAL) {
		ParseError(fs->lex, "There is too many Upvalues.\n");
	}

	if(fs->nuv >= fs->p->sizeuv) {
		size_t new_size = (fs->nuv+1)*2;
		fs->p->upvalues = RREALLOC(fs->r, TString *, fs->p->upvalues, fs->p->sizeuv, new_size);
		for(i = fs->nuv; i < new_size; ++i) {
			fs->p->upvalues[i] = NULL;
		}
		fs->p->sizeuv = new_size;
	}
	fs->p->upvalues[fs->nuv] = cast(TString *,name);
	fs->upvalues[fs->nuv].k = v->tt;
	fs->upvalues[fs->nuv].info = v->i;

	// 脚本函数里的upvalue，不需要gc
	rbtC_stable(cast(GCObject *, name));

	return fs->nuv++;
}

static void mark_upval( FuncState * fs, int level )
{
	if(level > fs->uv_level) {
		fs->uv_level = level;
	}
}

static int name_var_aux( FuncState * fs, expression * v, int b )
{
	if(!fs) {
		v->tt = EXP_GLOBAL;

		return EXP_GLOBAL;
	}

	const TString * name = strvalue(&fs->lex->token.tv);

	int m = search_var( fs, name );

	if(m >= 0) {
		if(b != 0) {
			mark_upval( fs, m );
		}

		v->tt = EXP_LOCAL;
		v->i = m;

		return EXP_LOCAL;
	}

	if(name_var_aux(fs->parent,v,b+1) == EXP_GLOBAL) {
		return EXP_GLOBAL;
	}

	int i = indexupvalue(fs, name, v);

	v->tt = EXP_UPVAL;
	v->i = i;

	return EXP_UPVAL;
}

static int addk( FuncState * fs, TValue * key, TValue * val )
{
	TValue * tv = rbtH_set(fs->r, fs->p->h, key);
	if(ttisnum(tv)) {
		return numvalue(tv);
	}

	setnumvalue(tv,fs->nk);
	if(fs->nk >= fs->p->sizek) {
		size_t new_size = (fs->nk + 1) * 2;
		fs->p->k = RREALLOC(fs->r,TValue,fs->p->k,fs->p->sizek,new_size);
		int i;
		for(i = fs->p->sizek; i < new_size; ++i) {
			setnilvalue(&fs->p->k[i]);
		}
		fs->p->sizek = new_size;
	}
	setvalue(&fs->p->k[fs->nk],val);

	// 脚本函数里的常量，需要始终在内存，不需要回收
	if(is_collectable(val)) {
		rbtC_stable(gcvalue(val));
	}

	return fs->nk++;
}

static int name_var( lexical * lex, expression * v )
{
	FuncState * fs = lex->fs;

	if(name_var_aux(fs,v,0) == EXP_GLOBAL) {
		v->i = addk(fs, &lex->token.tv, &lex->token.tv);
	}

	return 0;
}

static int constructor( lexical * lex, expression * v )
	// key-value := key : value | value
	// key-value-list := key-value {, key-value-list }
	// constructor := [] | [key-value-list]
{
	checknext( lex, LEX_LBRACKET );	// skip '['

	v->tt = EXP_NEW_TABLE;
	code_exp2nextreg(lex->fs, v);

	expression key, val;

	do {
		if(test(lex, LEX_RBRACKET)) {
			break;
		}

		expr(lex, &key);

		if(testnext(lex, LEX_COLON)) {
			// 'key : value'
			code_exp2nextreg(lex->fs, &key);

			expr(lex, &val);
			code_exp2nextreg(lex->fs, &val);

			code_set_table(lex->fs, v->i, key.i, val.i);

			lex->fs->freereg -= 2;

		} else {
			code_exp2nextreg(lex->fs, &key);

			code_set_table_next_num(lex->fs, v->i, key.i);

			lex->fs->freereg --;
		}

	} while( testnext(lex, LEX_COMMA) );

	checknext( lex, LEX_RBRACKET ); // skip ']'

	return v->i;
}

static int prefixexp( lexical * lex, expression * v )
	// prefixexp := NAME | ( expr ) | [] -- create a new array
{
	switch( lex->token.tt ) {
		case LEX_LPARENTHESES:
			lex_next_token( lex );
			expr(lex,v);
			checknext(lex, LEX_RPARENTHESES);
			break;

		case LEX_NAME:
			name_var( lex, v );
			lex_next_token( lex );
			break;

		case LEX_LBRACKET:
			//lex_next_token( lex );
			//checknext( lex, LEX_RBRACKET);	// []
			//v->tt = EXP_NEW_TABLE;
			constructor( lex, v );
			break;

		default:
			ParseError(lex,"Expect '( expr )' or 'Name', Get %s\n",lex_lex_str[lex->token.tt]);
			break;
	}

	return 0;
}

static int primaryexp( lexical * lex, expression * v )
	// primaryexp := prefixexp { '.'Name | '[' expr ']' | call function }
{
	FuncState * fs = lex->fs;
	prefixexp(lex, v);
	for(;;) {
		expression e;
		int i;
		int tt;
		switch( lex->token.tt ) {
			case LEX_DOT:
			//	code_exp2nextreg( lex->fs, v );
				code_exp2reg(lex->fs, v);
				lex_next_token(lex);	// skip '.'
				i = addk(lex->fs, &lex->token.tv, &lex->token.tv);
			//	i = code_loadk(lex->fs, i);
				tt = v->tt;
				v->tt = EXP_TABLE_INDEX;

				v->ii = v->i;	// v.ii is Table
				v->ka = 0;
				v->ra = (tt == EXP_LOCAL) ? 1 : 0;

				v->i = i;	// v.i is key, in stack
				v->kb = 1;
				v->rb = 0;

				lex_next_token(lex);
				
				break;

			case LEX_LBRACKET:
			//	code_exp2nextreg( lex->fs, v );
				code_exp2reg(lex->fs, v);
				lex_next_token(lex);	// skip '['
				if(testnext(lex, LEX_RBRACKET)) {
					v->tt = EXP_TABLE_NEXT_NUM;
					v->ii = v->i;
					v->ra = (v->tt == EXP_LOCAL) ? 1 : 0;
					v->ka = 0;
					break;
				}

				expr(lex, &e);
				checknext(lex, LEX_RBRACKET);	// check ']'

			//	i = code_exp2nextreg( lex->fs, &e );
				i = code_exp2reg(lex->fs, &e);

				tt = v->tt;
				v->tt = EXP_TABLE_INDEX;
				v->ii = v->i;
				v->ra = (tt == EXP_LOCAL) ? 1 : 0;
				v->ka = 0;

				v->i = i;
				v->rb = (e.tt == EXP_LOCAL) ? 1 : 0;
				v->kb = IS_CONSTANT(e.tt) ? 1 : 0;

				break;

			case LEX_LPARENTHESES:
				lex_next_token( lex );	// skip '('
				code_exp2nextreg( lex->fs, v );
				code_init_call(lex->fs, v->i);
				if(testnext(lex, LEX_RPARENTHESES)) {
					// 参数为空
				} else {
					explist( lex );
					checknext(lex, LEX_RPARENTHESES);
				}

				code_call(lex->fs,v->i);
				code(lex->fs, SetABC(MOV, v->i, v->i + 1, 0));	// 函数调用结束后，返回5个返回值，临时实现XXX
				code(lex->fs, SetABC(MOV, v->i+1, v->i + 2, 0));
				code(lex->fs, SetABC(MOV, v->i+2, v->i + 3, 0));
				code(lex->fs, SetABC(MOV, v->i+3, v->i + 4, 0));
				code(lex->fs, SetABC(MOV, v->i+4, v->i + 5, 0));

				v->tt = EXP_CALL;

				lex->fs->freereg = v->i + 1;

				break;

			default:
				return 0;
		}
	}
}

static int body( lexical * lex, expression * v )
	// used in local fun = function body
{
	FuncState fs;
	int i = open_func(lex->S->r,&fs,lex);

	checknext(lex,LEX_LPARENTHESES);
	do {
		if(test(lex,LEX_RPARENTHESES)) {
			break;
		}
		check(lex,LEX_NAME);

		new_local_var(lex);
		lex->fs->freereg++;

		lex_next_token( lex );

	}while(testnext(lex,LEX_COMMA));

	checknext(lex,LEX_RPARENTHESES);

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	}

	block(lex, NULL, BLOCK_TYPE_FUNCTION, use_brace);

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex,LEX_END);
	}

	close_func(lex->fs);

	v->i = code_closure( lex->fs, i, 0 );
	v->tt = EXP_NORELOCATE;

	return 0;
}

static int simple_expr( lexical * lex, expression * v )
	// simple_expr := { NUMBER | STRING | NIL | FALSE | TRUE | FUNCTION body | primaryexp }
{
	switch(lex->token.tt) {
		case LEX_NUMBER:
			v->tt = EXP_NUMBER;
			v->i = addk(lex->fs,&lex->token.tv,&lex->token.tv);
			break;

		case LEX_STRING:
			v->tt = EXP_CONSTANT;
			v->i = addk(lex->fs,&lex->token.tv,&lex->token.tv);
			break;

		case LEX_NIL:
			v->tt = EXP_NIL;
			break;
			
		case LEX_FALSE:
			v->tt = EXP_FALSE;
			break;

		case LEX_TRUE:
			v->tt = EXP_TRUE;
			break;

		case LEX_FUNCTION:
			lex_next_token( lex );
			return body( lex, v );

		default:
			return primaryexp( lex, v );
	}

	lex_next_token( lex );

	return 0;
}

static int subexpr( lexical * lex, expression * v, int priority )
	// expr := (UnaryOpr expr | simple_expr) [ BinaryOpr expr]
{
	int uop = get_unary_oprator( lex );
	if(uop != NOT_UOP) {
		lex_next_token( lex );
		subexpr(lex, v, UNARYOPR_PRIORITY);
		prefix(lex,v,uop);
	} else {
		simple_expr(lex,v);
	}

	int bop = get_binary_oprator( lex );
	while(bop != NOT_BOP && biop_priority[bop] > priority) {

		if((bop == BOP_AND) || (bop == BOP_OR)) {
			code_exp2nextreg(lex->fs, v);
		} else {
			code_exp2reg(lex->fs,v);
		}
		int pc = lex->fs->pc;

		lex_next_token( lex );
		expression v2;
		int op = subexpr(lex, &v2, biop_priority[bop]);
		infix(lex, v, &v2, bop, pc);
		bop = op;
	}

	return bop;
}

static int expr( lexical * lex, expression * v )
{
	subexpr(lex, v, 0);
	return 0;
}

static int assignment( lexical * lex, expression * e, int n )
	// assignment := expr list [ '=' explist ] ;
{
	int m = -1;
	if(test(lex,LEX_ASSIGN)) {
		lex_next_token( lex );
		m = explist( lex );

		if( m > n ) {
			ParseWarn(lex,"Assignment. Lose Expression(%d).\n",m-n);
			lex->fs->freereg -= m-n;
			m = n;
		}
		if( m < n ) {
			ParseWarn(lex, "Assignment. Values(%d) are less than Variables(%d)\n", m, n);
			lex->fs->freereg += n-m;
			m = n;
		}
	} else{
		checknext(lex, LEX_COMMA);
		expression  e2;
		expr(lex, &e2);
		m = assignment( lex, &e2, n+1 );
	}

	if(m == n) {
		storetovar(lex->fs,e,--lex->fs->freereg);
		return m-1;
	} else {
		storenil(lex->fs,e);
		return m;
	}
}

static int expr_stat( lexical * lex )
	// expr_stat := function | assignment | explist
{
	assert_freereg(lex);

	int freereg = lex->fs->freereg;

	expression e;
	primaryexp( lex, &e );
	
	if(e.tt == EXP_CALL) {
		lex->fs->freereg = freereg;
		return 0;
	}

	assignment( lex, &e, 1 );

	lex->fs->freereg = freereg;

	assert_freereg(lex);

	return 0;
}

static int explist( lexical * lex )
	// explist := expr [',' exprlist]
{
	int nexp = 1;
	expression e;
	expr(lex, &e);
	while( lex->token.tt == LEX_COMMA ) {
		lex_next_token( lex );
		code_exp2nextreg(lex->fs,&e);
		expr(lex,&e);
		nexp++;
	}

	code_exp2nextreg( lex->fs, &e );

	return nexp;
}

// 因为局部变量在栈上的位置是按照局部变量出现的顺序依次占位的
// 有些时候，需要创建一个*假*的，或*隐性*的局部变量，占据栈的一个位置
static int add_fake_local_val(lexical * lex)
{
	FuncState * fs = lex->fs;
	if(fs->nlocvar >= fs->p->sizelocvars) {
		size_t new_size = (fs->nlocvar + 1)*2;
		fs->p->locvars = RREALLOC(fs->r,LocVar,fs->p->locvars,fs->p->sizelocvars,new_size);
		fs->p->sizelocvars = new_size;

		if(new_size >= MAXLOCALVAL) {
			ParseError(lex, "There is too many local variables.\n");
		}
	}
	// 隐性的局部变量，不需要变量名
	fs->p->locvars[fs->nlocvar].name = NULL;

	return fs->nlocvar++;
}

static int new_local_var( lexical * lex )
{
	const TString * name = strvalue(&lex->token.tv);
	FuncState * fs = lex->fs;
	int i;
	for(i = 0; i < fs->nlocvar; ++i) {
		if(fs->p->locvars[i].name == name) {
			ParseError(lex,"Var(%s) already exists.\n",rbtS_gets(name));
		}
	}
	if(fs->nlocvar >= fs->p->sizelocvars) {
		size_t new_size = (fs->nlocvar + 1)*2;
		fs->p->locvars = RREALLOC(fs->r,LocVar,fs->p->locvars,fs->p->sizelocvars,new_size);
		fs->p->sizelocvars = new_size;

		if(new_size >= MAXLOCALVAL) {
			ParseError(lex, "There is too many local variables.\n");
		}
	}
	fs->p->locvars[fs->nlocvar].name = cast(TString *, name);

	// 脚本函数的局部变量的名称，始终在内存，不需要回收
	rbtC_stable(cast(GCObject *, name));

	return fs->nlocvar++;
}

static int local_func( lexical * lex )
{
	assert_freereg(lex);

	FuncState fs;
	lex_next_token( lex );	// skip function

	TString * fname = cast(TString *, rbtS_new(lex->S->r, ""));

	if(lex->token.tt == LEX_NAME) {
		fname = strvalue(&lex->token.tv);
		new_local_var( lex );
		lex_next_token( lex );
	} else {
		ParseWarn(lex,"Local Function Miss Name");
	}
	int i = open_func(lex->S->r, &fs, lex);

	fs.p->file = lex->filename;
	fs.p->fname = fname;
	rbtC_stable(cast(GCObject *, fname));

	checknext(lex, LEX_LPARENTHESES);
	
	do{
		if(test(lex,LEX_RPARENTHESES)) {
			break;
		}

		check(lex, LEX_NAME);

		new_local_var( lex );
		lex->fs->freereg++;

		lex_next_token( lex );
	}while( testnext(lex, LEX_COMMA) );

	checknext(lex, LEX_RPARENTHESES);

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	}

	block( lex, NULL, BLOCK_TYPE_FUNCTION, use_brace );

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex, LEX_END);
	}

	close_func( lex->fs );

	code_closure( lex->fs, i, 0 );

	assert_freereg(lex);

	return 0;
}

static int local_stat( lexical * lex )
	// local_stat := local v1,v2,v3,...,vk [;|=exprlist]
{
	FuncState * fs = lex->fs;

	lex_next_token( lex );	// skip 'local'

	if(lex->token.tt == LEX_FUNCTION) {
		return local_func( lex );
	}

	assert_freereg(lex);

	int nvar = 0;
	do {
		if(lex->token.tt != LEX_NAME) {
			ParseError(lex,"'local' Expect 'Name'.\n");
			return -1;
		}
		new_local_var( lex );
		nvar++;
		lex->fs->freereg++;
		lex_next_token( lex );
	}while( testnext(lex, LEX_COMMA) );


	int nexp = 0;
	if( lex->token.tt == LEX_ASSIGN ) {
		lex_next_token( lex );
		nexp = explist( lex );
	}

	if(nexp > nvar) {
		lex->fs->freereg -= nexp - nvar;
	}
	if(nexp < nvar) {
	//	code_set_nil(lex->fs, lex->fs->freereg, nvar - nexp);
		lex->fs->freereg += nvar - nexp;
	}

	int i;
	for(i = 0; i < nvar; ++i) {
		code(lex->fs,SetABC(MOV, lex->fs->nlocvar - 1 - i, --lex->fs->freereg, 0));
	}

	assert_freereg(lex);

	return 0;
}

static int function_stat( lexical * lex ) {
	assert_freereg(lex);

	lex_next_token( lex );

	TString * fname = cast(TString *, rbtS_new(lex->S->r, ""));

	int i = -1;
	if(lex->token.tt == LEX_NAME) {
		fname = strvalue(&lex->token.tv);
		i = addk( lex->fs, &lex->token.tv, &lex->token.tv );
		lex_next_token( lex );
	} else {
		ParseWarn(lex,"Function miss Name.\n");
	}

	checknext( lex, LEX_LPARENTHESES );

	FuncState fs;
	int pi = open_func(lex->S->r,&fs,lex);

	fs.p->file = lex->filename;
	fs.p->fname = fname;

	rbtC_stable(cast(GCObject *, fname));

	int nparam = 0;

	do {
		if(test(lex, LEX_RPARENTHESES)) {
			break;
		}
		check(lex, LEX_NAME);

		new_local_var(lex);
		lex->fs->freereg++;

		nparam++;

		lex_next_token( lex );
	}while(testnext(lex,LEX_COMMA));

	fs.p->nparam = nparam;

	checknext(lex,LEX_RPARENTHESES);

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	}

	block( lex, NULL, BLOCK_TYPE_FUNCTION, use_brace );


	close_func( lex->fs );

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex, LEX_END);
	}

	if( i >= 0 ) {
		int r2 = code_closure(lex->fs, pi, 1);
		code_set_global(lex->fs,i,r2);
		lex->fs->freereg --;
	}

	assert_freereg(lex);

	return 0;
}

static int ret_stat( lexical * lex )
	// return [; | end | expr]
{
	lex_next_token( lex );
	if(lex->token.tt == LEX_SEMICOLON || lex->token.tt == LEX_END) {
		code_ret(lex->fs,0,0);
		return 0;
	}


	assert_freereg(lex);

	int first = lex->fs->freereg;
	int n = explist(lex);

	code_ret(lex->fs,first,n);

	lex->fs->freereg -= n;

	assert_freereg(lex);

	return 0;
}

static int global_stat( lexical * lex )
{
	lex_next_token( lex );	// skip global

	/* 目前只支持全局函数 
	   声明全局变量的话,
	   只要使用一个非局部变量的变量名称即可
	 */
	check(lex,LEX_FUNCTION);

	if(lex->token.tt == LEX_FUNCTION) {
		return function_stat( lex );
	}
	return 0;
}

static int if_stat( lexical * lex, struct Block * pblock )
	// if expr then block [else if expr then block ] else block end
	/*
	   	for example:
			if a < b  then
				...
			else
				...
			end
		==>
			(if a < b then)
				LT R(a) R(b) -> R(a)
				JTRUE(a)
				JMP to_else

					...

				JMP to_end
		to_else:
			(else)

					...

		to_end:
			(end)
	 */
{
	assert_freereg(lex);

	lex_next_token( lex );	// skip if
	expression e;
	expr(lex,&e);
	code_exp2nextreg(lex->fs,&e);

	code_jump(lex->fs,"true",e.i);
	int i = code_jump(lex->fs,"none",0);

	lex->fs->freereg--;	// 释放掉判断用的寄存器

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	} else {
		checknext(lex,LEX_THEN);
	}

	block( lex, pblock, BLOCK_TYPE_IF, use_brace );

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	}

	int jump_to_end[256];
	int jump_to_end_num = 0;

	jump_to_end[jump_to_end_num++] = code_jump(lex->fs,"none",0);

	code_patch_jmp(lex->fs,i,lex->fs->pc);

	while(testnext(lex, LEX_ELSEIF)) {
		code_patch_jmp(lex->fs, i, lex->fs->pc);
		expr(lex, &e);
		code_exp2nextreg(lex->fs, &e);

		code_jump(lex->fs, "true", e.i);
		i = code_jump(lex->fs, "none", 0);

		lex->fs->freereg--;

		if(use_brace) {
			checknext(lex, LEX_LBRACE);
		} else {
			checknext(lex, LEX_THEN);
		}

		block( lex, pblock, BLOCK_TYPE_IF, use_brace );

		if(use_brace) {
			checknext(lex, LEX_RBRACE);
		}

		jump_to_end[jump_to_end_num++] = code_jump(lex->fs, "none", 0);
		if(jump_to_end_num >= 256) {
			ParseError(lex, "IF .. ELSEIF .. Too Many 'elseif'\n");
		}
	}

	code_patch_jmp(lex->fs, i, lex->fs->pc);

	if(testnext(lex,LEX_ELSE)) {
		if(use_brace) {
			checknext(lex, LEX_LBRACE);
		}

		block( lex, pblock, BLOCK_TYPE_IF, use_brace );

		if(use_brace) {
			checknext(lex, LEX_RBRACE);
		}
	}

	while(jump_to_end_num--) {
		code_patch_jmp(lex->fs,jump_to_end[jump_to_end_num],lex->fs->pc);
	}

	if(use_brace) {

	} else {
		checknext(lex,LEX_END);	// skip end
	}

	assert_freereg(lex);

	return 0;
}

static int for_stat( lexical * lex )
	// for exp1 ; exp2; exp3 do block end
	/*
	   		exp1						exp1
		check:(当exp2不空时)				check:(当exp2为空时)
			exp2						jump block
			if true jump block
			jump end

		inc:						inc:
			exp3						exp3
			jump check					jump check

		block:						block:
			
			...						...

			break; -- jump end				break; -- jump end

			...						...

			continue;-- jump inc				continue; -- jump inc

			...						...

			jump inc					jump inc

		end:						end:
	 */
{
	assert_freereg(lex);

	lex_next_token( lex );	// skip for

	expression e;
	if(lex->token.tt != LEX_SEMICOLON) {
		expr_stat(lex);
		//code_exp2nextreg(lex->fs, &e);
	}

	int check = lex->fs->pc;

	checknext(lex, LEX_SEMICOLON);

	int jump_end = -1;
	int jump_block = -1;
	if(lex->token.tt != LEX_SEMICOLON) {
		expr(lex, &e);
		code_exp2nextreg(lex->fs, &e);
		jump_block = code_jump(lex->fs, "true", e.i);
		jump_end = code_jump(lex->fs, "none",0);
	//	jump_block = code_jump(lex->fs, "none",0);

		lex->fs->freereg --;

	} else {
		jump_block = code_jump(lex->fs, "none",0);
	}

	checknext(lex, LEX_SEMICOLON);

	int inc = lex->fs->pc;
	expr_stat(lex);
	code_exp2nextreg(lex->fs, &e);
	int jump_check = code_jump(lex->fs, "none",0);

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	} else {
		checknext(lex, LEX_DO);
	}

	int blockpc = lex->fs->pc;
	struct Block B;
	B.bused = 0;
	B.cused = 0;
	block( lex, &B, BLOCK_TYPE_FOR, use_brace );

	int jump_inc = code_jump(lex->fs, "none", 0);

	if(jump_end >= 0) {
		code_patch_jmp(lex->fs, jump_end, lex->fs->pc);
	}
	int i;
	for(i = 0; i < B.bused; i++) {
		code_patch_jmp(lex->fs, B.breaks[i], lex->fs->pc);
	}

	for(i = 0; i < B.cused; i++) {
		code_patch_jmp(lex->fs, B.continues[i], inc);
	}

	if(jump_block >= 0) {
		code_patch_jmp(lex->fs, jump_block,blockpc);
	}

	code_patch_jmp(lex->fs, jump_check, check);

	code_patch_jmp(lex->fs, jump_inc, inc);

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex, LEX_END);
	}

	assert_freereg(lex);

	return 0;
}

static int while_stat( lexical * lex )
	// while exp do block end
	/*
		   check:
			exp
			if true skip next jump
			jump end

				...
			jump check

		   end:
	 */
{
	assert_freereg(lex);

	lex_next_token( lex );	// skip while

	int check = lex->fs->pc;

	expression e;
	expr(lex, &e);
	code_exp2nextreg(lex->fs, &e);
	code_jump(lex->fs, "true", e.i);
	lex->fs->freereg--;

	int jump_end = code_jump(lex->fs, "none", 0);

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	} else {
		checknext(lex, LEX_DO);
	}

	struct Block B;
	B.bused = 0;
	B.cused = 0;
	block( lex, &B, BLOCK_TYPE_WHILE, use_brace );

	int jump_check = code_jump(lex->fs, "none", 0);

	code_patch_jmp(lex->fs, jump_check, check);
	code_patch_jmp(lex->fs, jump_end, lex->fs->pc);

	int i;
	for(i = 0; i < B.bused; ++i) {
		code_patch_jmp(lex->fs, B.breaks[i], lex->fs->pc);
	}
	for(i = 0; i < B.cused; ++i) {
		code_patch_jmp(lex->fs, B.continues[i], check);
	}

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex, LEX_END);
	}

	assert_freereg(lex);

	return 0;
}

static int break_stat( lexical * lex )
{
	lex_next_token( lex );	// skip break

	return code_jump(lex->fs, "none", 0);
}

static int continue_stat( lexical * lex )
{
	lex_next_token( lex );	// skip continue 

	return code_jump(lex->fs, "none", 0);
}

static int foreach_stat( lexical * lex )
{
	assert_freereg(lex);

	lex_next_token( lex );	// skip foreach

	code_foreach_begin( lex->fs, lex->fs->freereg );
	lex->fs->freereg++;	// 给 idx = -1 留一个位置
	add_fake_local_val(lex);	// 新建一个假的局部变量，在栈上占据位置，防止被覆盖

	expression A;
	expr(lex, &A);
	code_exp2nextreg(lex->fs, &A);		// 将Table 放入idx后面的一个空，这是很关键的，因为opcode里不存idx的位置，只能根据Table的位置找到idx的位置
	add_fake_local_val(lex);	// 同样，防止被覆盖，新建一个假的局部变量

	checknext(lex, LEX_AS);	// skip as

	expression k, v;	// key 和 val
	expr(lex, &k);
	
	int has_key = 0;

	if(k.tt != EXP_LOCAL) {
		ParseError(lex, "Foreach. Key(Val) is not Local\n");
	}

	if(testnext(lex, LEX_COLON)) {	// skip :
		expr(lex, &v);
		if(v.tt != EXP_LOCAL) {
			ParseError(lex, "Foreach. Val is not Local\n");
		}
		has_key = 1;
	} else {
		has_key = 0;
		v = k;
	}

	int next;
	if(has_key) {
		next = code_foreach(lex->fs, A.i, k.i, v.i);
	} else {
		next = code_foreach_nokey(lex->fs, A.i, v.i);
	}

	int check = code_jump(lex->fs, "true", k.i);
	used(check);

	int jump_end = code_jump(lex->fs, "none", 0);

	//lex->fs->freereg--;	// 进入block之前，将数组A所占的寄存器释放掉(不需要释放)

	int use_brace = 0;
	if(testnext(lex, LEX_LBRACE)) {
		use_brace = 1;
	} else {
		checknext(lex, LEX_DO);
	}

	assert_freereg(lex);

	struct Block B;
	B.bused = 0;
	B.cused = 0;
	block( lex, &B, BLOCK_TYPE_FOR, use_brace );

	int jump_next = code_jump(lex->fs, "none", 0);

	code_patch_jmp(lex->fs, jump_next, next);
	code_patch_jmp(lex->fs, jump_end, lex->fs->pc);

	int i;
	for(i = 0; i < B.bused; ++i) {
		code_patch_jmp(lex->fs, B.breaks[i], lex->fs->pc);
	}
	for(i = 0; i < B.cused; ++i) {
		code_patch_jmp(lex->fs, B.continues[i], next);
	}

	if(use_brace) {
		checknext(lex, LEX_RBRACE);
	} else {
		checknext(lex, LEX_END);
	}

	// foreach 结束后，释放 idx 和 Array 占得位置(idx 不能释放！ Table 也不能释放)
	//lex->fs->freereg -= 2;
//	lex->fs->freereg -= 1;

	assert_freereg(lex);

	return 0;
}

static int parse_include( lexical * lex, const char * name, Table * env )
{
	struct stat file_stat;
	if(stat(name, &file_stat) < 0) {
		ParseWarn(lex, "Include (%s).Stat Error \n", name); 
		return 0;
	}

	if(S_ISREG(file_stat.st_mode)) {
		int len = strlen(name);
		if(len <= 4) {
	//		ParseWarn(lex, "Include(%s). File Must Be '.orz'\n", name);
			return 0;
		}
		if(name[len-1] != 'z' || name[len-2] != 'r' || name[len-3] != 'o' || name[len-4] != '.') {
	//		ParseWarn(lex, "Include(%s). File Must Be '.orz'\n", name);
			return 0;
		}

		stream * st = stream_open(lex->S->r, name);
		if(!st) {
			ParseWarn(lex, "Incude(%s). File Does Not Exist\n", name);
			return 0;
		}

		return script_parse(lex->S, st, env);
	}

	if(S_ISDIR(file_stat.st_mode)) {
		DIR * dir;
		struct dirent * dp;
		if((dir = opendir(name)) == NULL) {
			ParseWarn(lex, "Include (%s). Open Dir Error\n", name);
			return 0;
		}

		char file_name[1024];
		while( (dp = readdir(dir)) != NULL) {
			if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
				continue;
			}

			Table * env_sec = rbtH_init(lex->S->r, 1, 1);

			const char * p = dp->d_name;
			int len = strlen(p);
			if(len > 4) {
				if(p[len-1] == 'z' && p[len-2] == 'r' && p[len-3] == 'o' && p[len-4] == '.') {
					const TString * ts = rbtS_init_len(lex->S->r, p, len - 4);
					p = rbtS_gets(ts);
				}
			}

			settblvalue(rbtH_setstr(lex->S->r, env, p), env_sec);
			settblvalue(rbtH_setstr(lex->S->r, env_sec, "parent"), env);

			memset(file_name, 0, sizeof(file_name));
			snprintf(file_name, 1023, "%s/%s", name, dp->d_name);
			parse_include(lex, file_name, env_sec);
		}

		closedir(dir);
	}

	return 0;
}

static Table * include_build_env( lexical * lex, const char * fname )
{
	char * substr = strtok(cast(char*, fname), "/");
	Table * env, * global;
	global = lex->current_global;
	while(substr) {
		env = rbtH_init(lex->S->r, 1, 1);

		int len = strlen(substr);
		if(len > 4) {
			if(substr[len-1] == 'z' && substr[len-2] == 'r' && substr[len-3] == 'o' && substr[len-4] == '.') {
				const TString * ts = rbtS_init_len(lex->S->r, substr, len - 4);
				substr = cast(char*, rbtS_gets(ts));
			}
		}

		settblvalue(rbtH_setstr(lex->S->r, global, substr), env);
		settblvalue(rbtH_setstr(lex->S->r, env, "parent"), global);
		substr = strtok(NULL, "/");
		global = env;
	}

	return global;
}

static int include_stat( lexical * lex )
{
	lex_next_token( lex );	// skip include

	if(test(lex, LEX_STRING)) {
		lex_next_token(lex);
		return 0;

		TString * fname = strvalue(&lex->token.tv);

		if(strstr(rbtS_gets(fname), "./") || strstr(rbtS_gets(fname), "../")) {
			ParseWarn(lex, "Include(%s). './' and '../' is not allowed in the path\n", rbtS_gets(fname));
			return 0;
		}

		char long_file_name[1024];
		memset(long_file_name, 0, sizeof(long_file_name));

		snprintf(long_file_name, 1023, "%s/%s", rbtS_gets(lex->directory), rbtS_gets(fname));

		Table * env = include_build_env(lex, rbtS_gets(fname));

		parse_include(lex, long_file_name, env);

		lex_next_token( lex );

	} else {
		ParseWarn(lex, "Include Expect 'filename' or 'directory name'\n");
	}


	return 0;
}

int block( lexical * lex, struct Block * pblock, int block_type, int use_brace )
{
	int r = -1;

	while( lex->token.tt != LEX_END && lex->token.tt != LEX_EOF && lex->token.tt != LEX_RBRACE ) {
		switch( lex->token.tt ) {
			case LEX_ERROR:
				r = -1;
				ParseError(lex, "Lex Parse Error\n");
				break;

			case LEX_ELSE:
			case LEX_ELSEIF:
				if(block_type != BLOCK_TYPE_IF) {
					ParseError(lex, "UnExpect 'Else' or 'ElseIf' Without 'If'\n");
				}
				return 0;
				break;

			case LEX_AS:
				if(block_type != BLOCK_TYPE_FOR) {
					r = -1;
					ParseError(lex, "UnExpect 'AS' Without 'Foreach'\n");
				}
				break;

			case LEX_THEN:
				r = -1;
				ParseError(lex, "UnExpect 'Then' Without 'Do'\n");
				break;

			case LEX_LOCAL:
				r = local_stat( lex );
				break;
			case LEX_GLOBAL:
				r = global_stat( lex );
				break;
			case LEX_FUNCTION:
				r = function_stat( lex );
				break;
			case LEX_RETURN:
				r = ret_stat( lex );
				break;
			case LEX_IF:
				r = if_stat( lex, pblock );
				break;
			case LEX_FOR:
				r = for_stat( lex );
				break;
			case LEX_WHILE:
				r = while_stat( lex );
				break;
			case LEX_FOREACH:
				r = foreach_stat( lex );
				break;
			case LEX_SEMICOLON:
				r = 0;
				lex_next_token(lex);	// skip ';'
				break;
			case LEX_BREAK:
				r = break_stat( lex );
				if(pblock) {
					if(pblock->bused >= BLOCK_MAX_BREAK_CONTINUE) {
						ParseError(lex, "Too Many Break(%d) in one block.\n", pblock->bused + 1);
						break;
					}
					pblock->breaks[pblock->bused++] = r;
				}

				break;

			case LEX_CONTINUE:
				r = continue_stat( lex );
				if(pblock) {
					if(pblock->cused >= BLOCK_MAX_BREAK_CONTINUE) {
						ParseError(lex, "Too Many Continue(%d) in one block.\n", pblock->cused + 1);
						break;
					}
					pblock->continues[pblock->cused++] = r;
				}

				break;

			case LEX_INCLUDE:
				r = include_stat( lex );
				break;

			default:
				r = expr_stat( lex );
				break;
		}
		if(r < 0) {
			die(lex->S, "Block Parse Error.\n");
		}
	}
	if(lex->token.tt == LEX_RBRACE && !use_brace) {
		ParseError(lex, "Unexpect '}'\n");
	}
	if(lex->token.tt == LEX_END && use_brace) {
		ParseWarn(lex, "Expect '}' Got 'End'\n");
	}

	if(lex->token.tt == LEX_ERROR) {
		die(lex->S, "Lex parse Error.\n");
	}

	return 0;
}

