#include "code.h"
#include "object.h"

#include "parser.h"

#include "opcode.h"
#include "math.h"
#include "rabbit.h"

static int nclosure = 0;
static int ngl_closure = 0;

int code_get_closure(int * ncl, int * ngl)
{
	*ncl = nclosure;
	*ngl = ngl_closure;
	return 0;
}

static int erase( FuncState * fs, int pc)
{
	int c = pc;
	while(c + 1 < fs->pc) {
		fs->p->i[c] = fs->p->i[c+1];
		fs->p->line[c] = fs->p->line[c+1];
		c++;
	}
	fs->pc--;
	return pc;
}

static int insert( FuncState * fs, Instruction i, int pc )
{
	if(pc > fs->pc) {
		return 0;
	}

	if(fs->pc >= fs->p->sizei) {
		kLOG(fs->r, 0, "Insert PC. no mem? fs->pc(%d) vs fs->p->sizei(%zu)\n", fs->pc, fs->p->sizei);
		exit(0);
	}

	int c = fs->pc;
	while(c >= pc) {
		fs->p->i[c+1] = fs->p->i[c];
		fs->p->line[c+1] = fs->p->line[c];
		c--;
	}

	fs->p->i[pc] = i;
	fs->p->line[pc] = fs->lex->line;

	fs->pc++;

	return pc;
}

int code( FuncState * fs, Instruction i )
{
	if(fs->pc + 20 >= fs->p->sizei) {
		size_t new_size = fs->pc + 64;
		fs->p->i = RREALLOC(fs->r,Instruction,fs->p->i,fs->p->sizei,new_size);
		fs->p->line = RREALLOC(fs->r, int, fs->p->line, fs->p->sizei, new_size);
		fs->p->sizei = new_size;

	}
	fs->p->i[fs->pc] = i;
	fs->p->line[fs->pc] = fs->lex->line;

	return fs->pc++;
}

int code_set_nil( FuncState * fs, int startreg, int n )
{
	return code(fs, SetABC(SETNIL,startreg,n,0));
}

int code_exp2reg( FuncState * fs, expression * e )
{
	if(EXP2REG_DONTMOVE(e->tt)) {
		return e->i;
	}
	return code_exp2nextreg(fs, e);
}

int code_exp2nextreg( FuncState * fs, expression * e )
{
	int reg = fs->freereg;
	switch( e->tt ) {
		case EXP_LOCAL:
			if(reg != e->i) {
				code(fs,SetABC(MOV,reg,e->i,0));
			}
			break;

		case EXP_GLOBAL:
			code(fs,SetABC(GETGLOBAL,0,e->i,reg));
			break;

		case EXP_UPVAL:
			code(fs,SetABC(GETUPVAL,0,e->i,reg));
			break;

		case EXP_NUMBER:
			code(fs,SetABC(LOADK,reg,e->i,0));
			break;

		case EXP_CONSTANT:
			code(fs,SetABC(LOADK,reg,e->i,0));
			break;

		case EXP_NIL:
			code_set_nil(fs,reg,1);
			break;

		case EXP_FALSE:
			code(fs,SetABC(SETFALSE,reg,0,0));
			break;

		case EXP_TRUE:
			code(fs,SetABC(SETTRUE,reg,0,0));
			break;

		case EXP_TABLE_INDEX:
#define IS_A(e)	(e->ra || e->ka)
#define IS_B(e)	(e->rb || e->kb)
			if(IS_A(e) && IS_B(e)) {
				reg = fs->freereg;
			} else
			if(IS_A(e)) {
				reg = e->i;
			} else
		        if(IS_B(e)){
				reg = e->ii;
			} else {
				reg = min(e->ii, e->i);
			}
			fs->freereg = reg + 1;
			{
				Instruction i = SetABC(GETTABLE, e->ii, e->i, reg);
				SetKaKb(i, e->ka, e->kb);
				code(fs, i);
			}
		//	code(fs,SetABC(GETTABLE,e->ii, e->i,reg));
			e->tt = EXP_NORELOCATE;
			e->i = reg;
#undef IS_A
#undef IS_B
			return reg;

		case EXP_NORELOCATE:
			return e->i;

		case EXP_CALL:	// 将结果取回来, 覆盖函数本身 -- e->i 是函数本身的栈位置
		//	code_call(fs, e->i);
		//	code(fs, SetABC(MOV, e->i, e->i+1, 0));
		//	break;
			return e->i;

		case EXP_NEW_TABLE:
			code(fs, SetABC(NEWTABLE, 0, 0, reg));
			break;

		case EXP_TABLE_NEXT_NUM:
			kLOG(fs->r, 0, "Get Table Value miss KEY\n");
			break;
			
		default:
			return 0;
	}
	e->tt = EXP_NORELOCATE;
	e->i = reg;

	fs->freereg++;

	return reg;
}

int code_new_table( FuncState * fs )
{
	int reg = fs->freereg++;

	code(fs, SetABC(NEWTABLE, 0, 0, reg));

	return reg;
}

int code_not( FuncState * fs, expression * e )
{
	code(fs,SetABC(NOT,e->i,0,0));
	return 0;
}

int code_length( FuncState * fs, expression * e )
{
	code(fs, SetABC(LEN, e->i, 0, 0));
	return 0;
}

int code_value( FuncState * fs, expression * e )
{
	code(fs, SetABC(VALUE, e->i, 0, 0));
	return 0;
}

int code_add( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(ADD,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_minus( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(MINUS,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_multi( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(MULTI,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_div( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(DIV,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_gt( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(GT,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_lt( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(LT,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_ge( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(GE,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_le( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(LE,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_eq( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(EQ,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_ne( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(NE,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_remainder( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(REMAINDER,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_and( FuncState * fs, expression * e, expression * e2, int pc, int reg )
{
	code(fs, SetABC(AND, e->i, e2->i, reg));
	insert(fs, SetABx(JTRUE, e->i, 1), pc);
	insert(fs, SetABx(JMP, 0, fs->pc - pc), pc + 1);

	return reg;
/*
	
	int jmp_len = fs->pc - pc;

	if(e->tt == EXP_LOCAL) {
		insert(fs, SetABx(JTRUE, e->i, 2), pc);
		insert(fs, SetABC(MOV, reg, e->i, 0), pc + 1);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 2);
	} else if (IS_CONSTANT(e->tt)) {
		insert(fs, SetABx(JTRUE, e->i, 2), pc);
		insert(fs, SetABC(LOADK, reg, e->i, 0), pc + 1);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 2);
	} else {
		insert(fs, SetABx(JTRUE, e->i, 1), pc);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 1);
	}

	return reg;*/
}

int code_or( FuncState * fs, expression * e, expression * e2, int pc, int reg )
{
	code(fs, SetABC(OR, e->i, e2->i, reg));

	insert(fs, SetABx(JFALSE, e->i, 1), pc);
	insert(fs, SetABx(JMP, 0, fs->pc - pc), pc + 1);
	
	return reg;
/*
	int jmp_len = fs->pc - pc;

	if(e->tt == EXP_LOCAL) {
		insert(fs, SetABx(JFALSE, e->i, 2), pc);
		insert(fs, SetABC(MOV, reg, e->i, 0), pc + 1);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 2);
	} else if(IS_CONSTANT(e->tt)) {
		insert(fs, SetABx(JFALSE, e->i, 2), pc);
		insert(fs, SetABC(LOADK, reg, e->i, 0), pc + 1);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 2);
	} else {
		insert(fs, SetABx(JFALSE, e->i, 1), pc);
		insert(fs, SetABC(JMP, 0, 0, jmp_len), pc + 1);
	}

	return reg;*/
}

int code_concatenate( FuncState * fs, expression * e, expression * e2, int reg )
{
	Instruction i = SetABC(CONCATENATE,e->i,e2->i,reg);
	int ka = IS_CONSTANT(e->tt) ? 1 : 0;
	int kb = IS_CONSTANT(e2->tt) ? 1 : 0;
	SetKaKb(i, ka, kb);

	code(fs, i);

	return reg;
}

int code_closure( FuncState * fs, int i, int is_global_fun )
{
	nclosure++;
	if(is_global_fun) {
		ngl_closure++;
		code(fs, SetABC(CLOSURE_GLOBAL, i, fs->freereg, 0));
	} else {
		code(fs, SetABC(CLOSURE, i, fs->freereg, 0));
	}
	int k;
	for(k = 0; k < fs->nuv; ++k) {
		upval_desc desc = fs->upvalues[k];
		if(desc.k == EXP_LOCAL) {
			code(fs, SetABC(MOV,k,desc.info,0));
		}
		if(desc.k == EXP_UPVAL) {
			code(fs, SetABC(GETUPVAL,k,desc.info,0));
		}
	}

	return fs->freereg++;
}

int code_loadk( FuncState * fs, int i )
{
	code(fs, SetABC(LOADK, fs->freereg, i, 0));

	return fs->freereg++;
}

int code_init_call( FuncState * fs, int reg )
{
	code(fs, SetABC(INIT_CALL, reg, 0, 0));

	return reg;
}

int code_call( FuncState * fs, int reg )
{
	code(fs, SetABC(CALL, reg, 0, 0));

	return reg;
}

int code_set_global( FuncState * fs, int r1, int r2 )
{
	code(fs, SetABC(SETGLOBAL, 0, r1, r2));

	return 0;
}

int code_ret( FuncState * fs, int first, int count )
{
	code(fs, SetABC(RETURN,first,count,0));

	return 0;
}

static int code_jump_optimize( FuncState * fs, Instruction i )
{
	if(fs->pc < 1) {
		return -1;
	}
	int last_i = fs->p->i[fs->pc - 1];
	int last_op = GetOP(last_i);
	int last_ra = GetA(last_i);
	int last_rb = GetB(last_i);
	int last_rc = GetC(last_i);
	int last_ka = GetKa(last_i);
	int last_kb = GetKb(last_i);

	int op = GetOP(i);
	int ra = GetA(i);
	int rb = GetBx(i);
	if(rb < -127 || rb > 127) {
		return -1;
	}

	if(last_op < GT || last_op > NE || last_rc != ra) {
		return -1;
	}

	Instruction new_i;
	if(op == JTRUE) {
		new_i = SetABC(last_op + JGT - GT, last_ra, last_rb, rb);
		SetKaKb(new_i, last_ka, last_kb);
		fs->p->i[fs->pc - 1] = new_i;
		return fs->pc - 1;
	}

	return -1;
}

// JTRUE JFALSE 默认跳过下一条指令
int code_jump( FuncState * fs, const char * cmd, int reg )
{
	Instruction i;
	int retval;
	if(strcmp(cmd, "true") == 0) {
		i = SetABx(JTRUE, reg, 1);
		retval = code_jump_optimize(fs, i);
		if(retval >= 0) {
			return retval;
		}
		return code(fs,SetABx(JTRUE,reg,1));
	} 
	if(strcmp(cmd,"false") == 0) {
		i = SetABx(JFALSE, reg, 1);
		retval = code_jump_optimize(fs, i);
		if(retval >= 0) {
			return retval;
		}
		return code(fs,SetABx(JFALSE,reg,1));
	}
	if(strcmp(cmd,"none") == 0) {
		return code(fs,SetABC(JMP,0,0,0));
	}

	return 0;
}

int code_patch_jmp( FuncState * fs, int n, int pc )
{
	if(pc - n >= 1024*32 || pc - n <= - 1024 * 32) {	// 2^15
			kLOG(fs->r, 0, "Error!!%s(%zu):Jump Too Far.\n", rbtS_gets(fs->lex->filename), fs->lex->line);
			exit(0);
	}

	Instruction * i = &fs->p->i[n];
	int op = GetOP(*i);
	if(op == JMP) {
		SetBx(*i,(signed short int)(pc - n));
	}
	if(op == JTRUE || op == JFALSE || (op >= JGT && op <= JNE)) {
		if(pc - n >= 127 || pc - n <= -127) {
			kLOG(fs->r, 0, "Error!!%s(%zu):Jump Too Far.\n", rbtS_gets(fs->lex->filename), fs->lex->line);
			exit(0);
		}

		if(op == JTRUE || op == JFALSE) {
			SetBx(*i, (signed short int)(pc -n -1));
		} else {
			SetC(*i,(signed short int)(pc - n - 1));
		}
	}	

	return 0;
}

static int storeto_locvar_optimize(FuncState * fs, expression * e, int reg)
{
	if(e->tt != EXP_LOCAL) {
		return 0;
	}

	Instruction * i;
	int pc = fs->pc - 1;
//	int ra, rb, rc, ka, kb;
	for(; pc >= 0; --pc) {
		i = &fs->p->i[pc];

		int op = GetOP(*i);
		if(OpCode_get_input1(*i) == reg || OpCode_get_input2(*i) == reg || OpCode_get_input3(*i) == reg) {
			// 之前有指令使用这个寄存器，不能优化！
			return 0;
		}
		int output = OpCode_get_output(*i);

		if(output < 0 || output != reg) {
			continue;
		}
		switch(OpCode_Info[op].output_reg) {
			case 1:
				SetA(*i, e->i);
				break;

			case 2:
				SetB(*i, e->i);
				break;

			case 3:
				SetC(*i, e->i);
				break;

			case 4:
				SetBx(*i, e->i);
				break;
		}
		return 1;
	}

	return 0;
}

static int storeto_table_optimize(FuncState * fs, expression * e, int reg)
{
	if(e->tt != EXP_TABLE_INDEX) {
		return 0;
	}

	Instruction * i;
	int pc = fs->pc - 1;
//	int ra, rb, rc, ka, kb;
	for(; pc >= 0; --pc) {
		i = &fs->p->i[pc];

		int op = GetOP(*i);

		if(OpCode_get_input1(*i) == reg || OpCode_get_input2(*i) == reg || OpCode_get_input3(*i) == reg) {
			// 之前有指令使用这个寄存器，不能优化！
			return 0;
		}

		int output = OpCode_get_output(*i);

		if(output < 0 || output != reg) {
			continue;
		}
		if(op == MOV && GetB(*i) < fs->nlocvar) {
			// 目标值是局部变量
			Instruction ii = SetABC(SETTABLE, e->ii, e->i, GetB(*i));
			int ka = 0;
			int kb = e->kb;
			SetKaKb(ii, ka, kb);
			SetKc(ii, 0);
			code(fs, ii);

			erase(fs, pc);
			return 1;
		}
		if(op == LOADK) {
			// 目标值是常量
			Instruction ii = SetABC(SETTABLE, e->ii, e->i, GetB(*i));
			int ka = 0;
			int kb = e->kb;
			SetKaKb(ii, ka, kb);
			SetKc(ii, 1);
			code(fs, ii);

			erase(fs, pc);

			return 1;
		}
		// 目标是其他值，无法优化（现在无法优化。。。）
		return 0;
	}

	return 0;
}

int storetovar( FuncState * fs, expression * e, int reg )
{
	if(storeto_locvar_optimize(fs, e, reg)) {
		return 0;
	}

	if(storeto_table_optimize(fs, e, reg)) {
		return 0;
	}

	switch( e->tt ) {
		case EXP_LOCAL:
			code(fs, SetABC(MOV, e->i, reg, 0));
			break;
			 
		case EXP_GLOBAL:
			code(fs, SetABC(SETGLOBAL, 0, e->i, reg));
			break;

		case EXP_UPVAL:
			code(fs, SetABC(SETUPVAL, 0, e->i, reg));
			break;

		case EXP_TABLE_INDEX:
			{
				Instruction i = SetABC(SETTABLE, e->ii, e->i, reg);
				int ka = 0;
				int kb = e->kb;
				SetKaKb(i, ka, kb);
				code(fs, i);
			//	code(fs, SetABC(SETTABLE, e->ii, e->i, reg));
			}
			break;

		case EXP_TABLE_NEXT_NUM:
			{
				Instruction i = SetABC(SETTABLE_NEXTNUM, e->ii, 0, reg);
				code(fs, i);
			}
		//	code(fs, SetABC(SETTABLE_NEXTNUM, e->ii, 0, reg));
			break;

		default:
			kLOG(fs->r, 0, "%s(%zu):Can not Store Value.\n", rbtS_gets(fs->lex->filename), fs->lex->line);
			break;
	}

	return 0;
}

int storenil( FuncState * fs, expression * e )
{
	switch( e->tt ) {
		case EXP_LOCAL:
			code(fs, SetABC(SETNIL,e->i,1,0));
			break;

		case EXP_GLOBAL:
			code(fs, SetABC(SETGLOBALNIL,e->i,0,0));
			break;

		case EXP_UPVAL:
			code(fs, SetABC(SETUPVALNIL,e->i,0,0));
			break;

		case EXP_TABLE_INDEX:
			{
				Instruction i = SetABC(SETTABLE_NIL, e->ii, e->i, 0);
				int ka = 0;
				int kb = e->kb;
				SetKaKb(i, ka, kb);
				code(fs, i);
			}
		//	code(fs, SetABC(SETTABLE_NIL, e->ii, e->i, 0));
			break;

		case EXP_TABLE_NEXT_NUM:
			break;

		default:
			kLOG(fs->r, 0, "%s(%zu): Can not Store nil.\n",rbtS_gets(fs->lex->filename), fs->lex->line);
			break;
	}
	return 0;
}

int code_set_table( FuncState * fs, int ra, int rb, int rc )
{
	return code(fs, SetABC(SETTABLE, ra, rb, rc));
}

int code_set_table_next_num( FuncState * fs, int ra, int rc )
{
	return code(fs, SetABC(SETTABLE_NEXTNUM, ra, 0, rc));
}

int code_foreach_begin( FuncState * fs, int ra )
{
	return code(fs, SetABC(FOREACH_BEGIN, ra, 0, 0));
}

int code_foreach( FuncState * fs, int ra, int rb, int rc )
{
	return code(fs, SetABC(FOREACH, ra, rb, rc));
}

int code_foreach_nokey( FuncState * fs, int ra, int rc) 
{
	return code(fs, SetABC(FOREACH_NOKEY, ra, 0, rc));
}

