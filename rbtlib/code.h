#ifndef _code_h_
#define _code_h_

#include "parser.h"

int code( FuncState * fs, Instruction opcode );

int code_set_nil( FuncState * fs, int startreg, int n );

int code_exp2nextreg( FuncState * fs, expression * e );

// 在运算表达式： A # B 这种表达式中，（#可以是+ - * /等），如果A、B是局部变量和常数的话，不需要将局部变量移到新的位置
#define EXP2REG_DONTMOVE(tt) ((tt) == EXP_LOCAL || (tt) == EXP_NUMBER || (tt) == EXP_CONSTANT)
int code_exp2reg( FuncState * fs, expression * e );

int code_not( FuncState * fs, expression * e );

int code_length( FuncState * fs, expression * e );

int code_value( FuncState * fs, expression * e );

int code_add( FuncState * fs, expression * e, expression * e2, int reg );

int code_minus( FuncState * fs, expression * e, expression * e2, int reg );

int code_multi( FuncState * fs, expression * e, expression * e2, int reg );

int code_div( FuncState * fs, expression * e, expression * e2, int reg );

int code_gt( FuncState * fs, expression * e, expression * e2, int reg );

int code_lt( FuncState * fs, expression * e, expression * e2, int reg );

int code_ge( FuncState * fs, expression * e, expression * e2, int reg );

int code_le( FuncState * fs, expression * e, expression * e2, int reg );

int code_eq( FuncState * fs, expression * e, expression * e2, int reg );

int code_ne( FuncState * fs, expression * e, expression * e2, int reg );

int code_remainder( FuncState * fs, expression * e, expression * e2, int reg );

int code_and( FuncState * fs, expression * e, expression * e2, int pc, int reg );

int code_or( FuncState * fs, expression * e, expression * e2, int pc, int reg );

int code_concatenate( FuncState * fs, expression * e, expression * e2, int reg );

int code_free_reg( FuncState * fs, int n );

int code_closure( FuncState * fs, int i, int is_global_fun );	// i -- index to Protos

int code_loadk( FuncState * fs, int i );	// i -- index to k

int code_init_call( FuncState * fs, int reg );
int code_call( FuncState * fs, int reg );

int code_set_global( FuncState * fs, int r1, int r2 );

int code_ret( FuncState * fs, int first, int count );

int code_jump( FuncState * fs, const char * cmd, int reg);

int code_patch_jmp( FuncState * fs, int i, int pc );

int storetovar( FuncState * fs, expression * e, int reg );

int storenil( FuncState * fs, expression * e );

int code_new_table( FuncState * fs );

int code_get_table( FuncState * fs, int ra, int rb, int rc );

int code_set_table( FuncState * fs, int ra, int rb, int rc );

int code_set_table_next_num( FuncState * fs, int ra, int rc );

int code_foreach_begin( FuncState * fs, int ra );

int code_foreach( FuncState * fs, int ra, int rb, int rc );

int code_foreach_nokey( FuncState * fs, int ra, int rc );

// debug
int code_get_closure(int * n, int * ng);

#endif

