#ifndef _opcode_h_
#define _opcode_h_

#include "script_struct.h"

/*
 *	Instruction is 32-bit unsigned int. The first significant byte is OP. The valid range of OP is from 0 to 63. 
 *	The first significant bit of OP is a flag (ka) which indicate whether RA is constant or register.
 *	The second significant bit of OP is a flag (kb) which indicate where RB is constant or register.
 *
 */

#define opcode_assert(x)	\
	do {	\
		if(!(x)) {	\
			ParseError(fs->lex, "The Function is Too Big!! Split it to some small Functions!(%s). LocVar(%d), K(%d)\n", #x, fs->nlocvar, fs->nk);	\
		}	\
	} while(0)

#define SetABC( o, a, b, c) ({	\
		Instruction ____i;	\
		int ____o = o;		\
		int ____a = a;		\
		int ____b = b;		\
		int ____c = c;		\
		opcode_assert((____o) < 64);	\
		opcode_assert((____a) < 128);	\
	       	opcode_assert((____b) < 128);	\
	       	opcode_assert((____c) < 128); 	\
		____i = cast(Instruction, (((____o)<<24)&0x3f000000) | (((____a)<<16)&0x7f0000) | (((____b)<<8)&0x7f00) | ((____c)&0x7f) );	\
		____i;	\
	}) 

#define SetABx( o, a, b ) ({	\
		Instruction ____i;	\
		int ____o = o;		\
		int ____a = a;		\
		int ____b = b;		\
		opcode_assert(____o < 64);	\
		opcode_assert(____a < 128);	\
	       	____i = cast(Instruction, (((____o)<<24)&0x3f000000) | (((____a)<<16)&0x7f0000) | ((____b)&0xffff) );	\
		____i;	\
	})

#define SetOP(i, op) {	\
	int ____o = op;		\
	opcode_assert(____o < 64);	\
	i = ((i) & 0x00ffffff) | ((____o) << 24 & 0x3f000000);	\
}

#define SetA( i, a ) {	\
	int ____a = a;	\
	opcode_assert(____a < 128);	\
	i = ((i) & 0xff00ffff) | ((____a) << 16 & 0x7f0000);	\
}

#define SetBx( i, b ) (i = ((i) & 0xffff0000) | (cast(signed short int,b) & 0xffff))

#define SetB( i, b ) {	\
	int ____b = b;	\
	opcode_assert((____b) < 128);	\
       	i = ((i) & 0xffff00ff) | ((____b) << 8 & 0x7f00);	\
}

#define SetC( i, c ) {	\
	int ____c = c;	\
	opcode_assert((____c) < 128);	\
	i = ((i) & 0xffffff00) | ((____c) & 0x7f);	\
}

#define SetKaKb(i, ka, kb) (i = ((i) & 0x3fffffff) | ((ka) << 31) | ((kb) << 30))
#define SetKc(i, kc) (i = ((i) & 0xffffff7f) | ((kc) << 7))

#define GetOP( i ) cast(unsigned int, ((i) >> 24) & 0x3f)
#define GetKa( i ) cast(unsigned int, ((i) >> 31) & 0x1)
#define GetKb( i ) cast(unsigned int, ((i) >> 30) & 0x1)
#define GetKc( i ) cast(unsigned int, ((i) >> 7) & 0x1)
#define GetA( i ) cast(unsigned int, ((i) >> 16) & 0x7f)
#define GetBx( i ) cast(signed short int, (i) & 0xffff)
#define GetB( i ) cast(unsigned int, ((i) >> 8) & 0x7f)
#define GetC( i ) cast(unsigned int, (i) & 0x7f)


typedef enum OpCode {
	MOV,		//	move RA RB 0, RA <-- RB				/* 0 */

	SETNIL,		//	setnil RA RB 0, stack[RA...RA+RB] = nil
	SETFALSE,	//	setfalse RA 0 0
	SETTRUE,	//	settrue RA 0 0

	LOADK,		//	loadk RA RB 0, RA <-- k[RB]

	ADD,		//	add RA RB RC, RA + RB --> RC			/* 5 */
	MINUS,		// 	minus RA RB RC, RA - RB --> RC
	MULTI,		// 	multi RA RB RC, RA * RB --> RC
	DIV,		//	div RA RB RC, RA / RB --> RC

	REMAINDER,	//	remainder RA RB RC , RA % RB --> RC		

	AND,		//	and RA RB RC, RA && RB --> RC			/* 10 */

	OR,		// 	or RA RB RC, RA || RB --> RC

	CONCATENATE,	//	concatenate RA RB RC. RA .. RB --> RC

	GT,		//	gt RA RB RC, RA > RB --> RC
	LT,		//	lt RA RB RC, RA < RB --> RC
	GE,		//	ge RA RB RC, RA >= RB --> RC			/* 15 */
	LE,		// 	le RA RB RC, RA <= RB --> RC
	EQ,		//	eq RA RB RC, RA == RB --> RC
	NE,		// 	ne RA RB RC, RA != RB --> RC

	NOT,		//	not RA 0 0, RA <-- not RA

	LEN,		//	len RA 0 0, RA <-- lenght of RA			/* 20 */

	CMP,		//	cmp RA RB RC, RA cmp RB --> RC
	JMP,		//	jmp 0 RBx, pc += RBx
	JTRUE,		// 	jtrue RA 0 0, if(RA == true) pc += RBx
	JFALSE,		//	jfalse RA 0 0, if(RA == false) pc += RBx

	JGT,		//	jgt RA RB RC, if(RA > RB) pc += RC		/* 25 */
	JLT,		//	jlt RA RB RC, if(RA < RB) pc += RC
	JGE,		//	jge RA RB RC, if(RA >= RB) pc += RC
	JLE,		// 	jle RA RB RC, if(RA <= RB) pc += RC
	JEQ,		// 	jz  RA RB RC, if(RA == RB) pc += RC
	JNE,		//	jnz RA RB RC, if(RA != RB) pc += RC		/* 30 */

	GETGLOBAL,	//	getglobal 0 RB RC, G[RB] --> RC	
	SETGLOBAL,	//	setglobal 0 RB RC, G[RB] <-- RC			
	SETGLOBALNIL,	//	setglobalnil RA 0 0,

	INIT_CALL,	//	init_call RA, 0, 0 (初始化函数调用，将参数初始化为nil)
	CALL,		//	call RA 0 0, RA()				/* 35 */

	RETURN,		//	return RA RB 0,	return stack[RA...RA+RB]
	CLOSE,		//	close upvalues					

	CLOSURE,	// 	closure RA RB 0, closure proto[RA] --> RB

	GETUPVAL,	//	getupval 0 RB RC, uv[RB] --> RC	
	SETUPVAL,	//	setupval 0 RB RC, uv[RB] <-- RC			/* 40 */
	SETUPVALNIL,	//	setupvalnil RA 0 0

	GETTABLE,	//	gettable RA RB RC, RA(tbl)[RB] --> RC
	SETTABLE,	//	settable RA RB RC, RA(tbl)[RB] <-- RC
	SETTABLE_NEXTNUM,//	settable_nextnum RA RB RC, RA(tbl)[next num] <-- RC

	SETTABLE_NIL,	//	settable_nil					/* 45 */

	NEWTABLE,	//	create table 0 0 reg, new table --> RC

	VALUE,		//	value RA 0 0, VALUE(RA(string)) --> RA		

	FOREACH_BEGIN,	//	foreach_begin RA 0 0 , RA is idx
	FOREACH,	//	foreach RA RB RC, RA is table, RB is key, RB is val
	FOREACH_NOKEY,	//	foreach RA 0 RC, RA is table, RC is val		/ * 50 */

	CLOSURE_GLOBAL,	// 	closure RA RB 0, closure proto[RA] --> RB	全局函数
}OpCode;

extern const char * OpCode_Str[];

struct OpInfo {
	int output;	// 指令是否有输出 
	int output_reg;	// 输出位置 1 : ra, 2 : rb, 3 : rc, 4 : rbx
	int input1;	// 指令的第一个输入 0 : 没有输入, 1 : ra, 2 : rb, 3 : rc, 4 : rbx
	int input2;
	int input3;
};

extern const struct OpInfo OpCode_Info[];

// 获取指令i，占用的栈空间
int OpCode_get_output(Instruction i);
int OpCode_get_input1(Instruction i);
int OpCode_get_input2(Instruction i);
int OpCode_get_input3(Instruction i);

#endif

