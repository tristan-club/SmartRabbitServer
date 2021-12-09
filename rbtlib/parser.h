#ifndef _parser_h_
#define _parser_h_

#include "stream.h"
#include "script.h"
#include "object.h"

#include "lexical.h"

enum {
	EXP_GLOBAL,
	EXP_LOCAL,
	EXP_UPVAL,
	EXP_CONSTANT,
	EXP_NUMBER,
	EXP_NIL,
	EXP_FALSE,
	EXP_TRUE,
	EXP_NORELOCATE,
	EXP_RELOCATABLE,
	EXP_CALL,
	EXP_TABLE_INDEX,
	EXP_TABLE_NEXT_NUM,
	EXP_NEW_TABLE,
};

#define IS_CONSTANT(tt)	((tt) == EXP_NUMBER || (tt) == EXP_CONSTANT)

#define MAXUPVAL 64

#define MAXLOCALVAL	255

typedef struct upval_desc {
	int k;
	int info;
}upval_desc;

typedef struct FuncState {
	Proto * p;

	struct FuncState * parent;      // 在哪个函数体里定义的函数
	lexical * lex;
	rabbit * r;

	int nlocvar;             // 局部变量的数量
	int pc;			// present instruction/code
	int nk;			// constant number
	int np;			// proto number
	//int nuvname;		// upval number

	int freereg;

	upval_desc upvalues[MAXUPVAL];
	int nuv;

	int uv_level;

}FuncState;

typedef struct expression {
	int tt;
	int i;
	int ii;
	double d;

	int ka, kb;	// EXP_TABLE_INDEX 时使用，表示寄存器A、B分别是不是常量
	int ra, rb;	// EXP_TABLE_INDEX 时使用，表示寄存器A、B分别是不是局部变量

	size_t pos;	// 函数内部,栈的位置
}expression;

int script_parse(Script * S, stream * s, Table * env);

#endif

