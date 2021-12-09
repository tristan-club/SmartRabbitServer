#include "vm.h"
#include "rabbit.h"
#include "code.h"
#include "opcode.h"
#include "table.h"
#include "string.h"
#include "proto.h"
#include "pool.h"
#include "table_struct.h"
#include "string_struct.h"
#include "string_for_vm.h"
#include "gc.h"

#define stack_at(ctx, n)	({TValue * ____r = &((ctx)->stack[n]); if((n)>=(128+10)){VMWarn(S, "stack_at(%d) is too big!", n);}; ____r;})

static void VMError( Script * S,  const char * format , ...)
{
	static char buffer[256];
	memset(buffer,0,256);

	va_list ap;
	va_start(ap,format);

	TString * fname = cast(TString *, rbtS_new(S->r, ""));
	const TString * file = cast(TString *, rbtS_new(S->r, ""));

	int line = 0;

	if(!S->ctx->cl->isC) {
		fname = S->ctx->cl->u.p->fname;
		file = S->ctx->cl->u.p->file;
		line = S->ctx->cl->u.p->line[S->ctx->cl->pc];
	} else if(S->ctx->cl->caller){
		fname = S->ctx->cl->caller->u.p->fname;
		file = S->ctx->cl->caller->u.p->file;
		line = S->ctx->cl->caller->u.p->line[S->ctx->cl->caller->pc];
	}

	int i = snprintf(buffer,255,"Error\t(Function<%s:%s> Line<%d>)", rbtS_gets(file), rbtS_gets(fname), line + 1);

	vsnprintf(buffer+i,255-i,format,ap);
	va_end(ap);

	die(S, buffer);
}

static void VMWarn( Script * S, const char * format, ... )
{
	static char buffer[256];
	memset(buffer,0,256);

	va_list ap;
	va_start(ap,format);

	TString * fname = cast(TString *, rbtS_new(S->r, ""));
	const TString * file = cast(TString *, rbtS_new(S->r, ""));

	int line = 0;

	if(!S->ctx->cl->isC) {
		fname = S->ctx->cl->u.p->fname;
		file = S->ctx->cl->u.p->file;
		line = S->ctx->cl->u.p->line[S->ctx->cl->pc];
	} else if(S->ctx->cl->caller){
		fname = S->ctx->cl->caller->u.p->fname;
		file = S->ctx->cl->caller->u.p->file;
		line = S->ctx->cl->caller->u.p->line[S->ctx->cl->caller->pc];
	}

	int i = snprintf(buffer,255,"WARN\t(Function<%s:%s> Line<%d>)", rbtS_gets(file), rbtS_gets(fname), line + 1);

	vsnprintf(&buffer[i],255-i,format,ap);

	va_end(ap);

//	puts(buffer);
	kLOG(S->r, 0, "%s", buffer);
}

void rbtScript_warn(Script * S, const char * msg)
{
	VMWarn(S, "%s\n", msg);
}

#define checktop(S,i) { if(check_stack(S,i) < 0) { VMError(S, "Access Illeage Stack. Size:%d,Top:%d,Access:%d.\n",(S)->ssize,(S)->top,i); }}

/*
 *	VM -- Table For VM
 */

#define vmH_find_str(ret, _r, _t, _str, _create)	\
	do {	\
		ret = NULL;	\
		rabbit * r = _r;	\
		Table * t = _t;	\
		const TString * str = _str;	\
		struct Node * n = gnode(t, str->hash % t->table_size);	\
		while(n) {	\
			if(ttisstr(gkey(n)) && cast(size_t, strvalue(gkey(n))) == cast(size_t, str)) {	\
				ret = gval(n);	\
				break;	\
			}	\
			n = n->next;	\
		}	\
		if(_create && !ret) {	\
			TValue key;	\
			setstrvalue(&key, str);	\
			ret = rbtHVM_newkey(r, t, &key);	\
		}	\
	}while(0)

#define vmH_find_num(ret, _r, _t, _n, _create)	\
	do {	\
		ret = NULL;	\
		rabbit * r = _r;	\
		Table * t = _t;	\
		int n = _n;	\
		if(n >= 0 && n < t->vector_size) {	\
			ret = &t->vector[n];	\
			break;	\
		}	\
		struct Node * node = gnode(t, abs(n) % t->table_size);	\
		while(node) {	\
			if(ttisnum(gkey(node)) && numvalue(gkey(node)) == n) {	\
				ret = gval(node);	\
				break;	\
			}	\
			node = node->next;	\
		}	\
		if(_create && !ret) {	\
			TValue key;	\
			setnumvalue(&key, n);	\
			ret = rbtHVM_newkey(r, t, &key);	\
		}	\
	}while(0)

#define vmH_find_fnum(ret, _r, _t, _d, _create)	\
	do {	\
		ret = NULL;	\
		rabbit * r = _r;	\
		Table * t = _t;	\
		double d = _d;	\
		assert(DOUBLE_SIZE == 8);	\
		union {	\
			double d;	\
			char c[DOUBLE_SIZE];	\
		}h;	\
		h.d = d;	\
		h.c[0] += h.c[1];	\
		h.c[0] += h.c[2];	\
		h.c[0] += h.c[3];	\
		h.c[0] += h.c[4];	\
		h.c[0] += h.c[5];	\
		h.c[0] += h.c[6];	\
		h.c[0] += h.c[7];	\
		unsigned int hash = cast(unsigned int, h.c[0]);	\
		struct Node * node = gnode(t, hash % t->table_size);	\
		while(node) {	\
			if(ttisfnum(gkey(node)) && fnumvalue(gkey(node)) == d) {	\
				ret = gval(node);	\
				break;	\
			}	\
			node = node->next;	\
		}	\
		if(_create && !ret) {	\
			TValue key;	\
			setfnumvalue(&key, d);	\
			ret = rbtHVM_newkey(r, t, &key);	\
		}	\
	}while(0)


/*
 *	-------------------------
 */

static inline void set_nil( Script * S, int first, int count )
{
	while(count > 0) {
		setnilvalue(stack_n(S,first+count-1));
		count --;
	}
}


static inline double obj2fnum( Script * S, const TValue * tv, const char * debug_msg )
{
	if(ttisfnum(tv)) {
		return fnumvalue(tv);
	}
	if(ttisnum(tv)) {
		return cast(double,numvalue(tv));
	}
	if(ttisstr(tv)) {
		return rbtS_tofnum(strvalue(tv));
	}

	VMWarn(S, "Object convert to Number. Precesion may be lost.(%s)\n", debug_msg);
	debug_tvalue_dump(cast(TValue*, tv));

	return 0;
}

static inline int vm_add_str( Script * S, const TValue * a, const TValue * b, TValue * result )
{
	const TString * ts_a = rbtO_rawToString(S->r, a);
	const TString * ts_b = rbtO_rawToString(S->r, b);

	const TString * ts_c = rbtS_concatenate(S->r, ts_a, ts_b);

	setstrvalue(result, ts_c);
	return 0;
}

#define vm_add_quick(S, a, b, out)	\
	do {	\
		double aa, bb;	\
		TValue * tv = out;	\
		if(likely(ttisfnum(a) && ttisfnum(b))) {	\
			aa = fnumvalue(a);	\
			bb = fnumvalue(b);	\
			setfnumvalue(tv, aa+bb);	\
			break;	\
		}	\
		if(likely(ttisnumber(a) && ttisnumber(b))) {	\
			aa = numbervalue(a);	\
			bb = numbervalue(b);	\
			setfnumvalue(tv, aa+bb);	\
			break;	\
		}	\
		if(likely(ttisstr(a) || ttisstr(b))) {	\
			vm_add_str(S, a, b, tv);	\
			break;	\
		}	\
		aa = obj2fnum(S, a, "add");	\
		bb = obj2fnum(S, b, "add");	\
		setfnumvalue(tv, aa + bb);	\
	}while(0)

static inline int vm_add( Script * S, TValue * a, TValue * b, TValue * result )
{
	if(likely(ttisfnum(a) && ttisfnum(b))) {
		setfloatvalue(result, fnumvalue(a) + fnumvalue(b));
		return 0;
	}
	if((ttisstr(a) || ttisstr(b))) {
		return vm_add_str( S, a, b, result );
	}

	double aa = obj2fnum( S, a, "add" );
	double bb = obj2fnum( S, b, "add" );

	setfloatvalue(result, aa + bb);

	return 0;
}

#define vm_minus_quick(S, a, b, result)	\
	do {	\
		double aa = obj2fnum(S, a, "minus");	\
		double bb = obj2fnum(S, b, "minus");	\
		setfloatvalue(result, aa - bb);	\
	} while(0)

static inline int vm_minus( Script * S, TValue * a, TValue * b, TValue * result )
{
	double aa = obj2fnum(S, a, "minus");
	double bb = obj2fnum(S, b, "minus");

	setfloatvalue(result, aa - bb);
	return 0;
}

static inline int vm_multi( Script * S, TValue * a, TValue * b, TValue * result )
{
	double aa = obj2fnum(S, a, "multi");
	double bb = obj2fnum(S, b, "multi");

	setfloatvalue(result, aa * bb);

	return 0;
}

#define vm_div_quick(S, a, b, result)	\
	do {	\
		double aa = obj2fnum(S, a, "div");	\
		double bb = obj2fnum(S, b, "div");	\
		if(unlikely(bb == 0)) {	\
			VMWarn(S, "Divide by Zero.\n");	\
			setfloatvalue(result, 0);	\
			break;	\
		}	\
		setfloatvalue(result, aa / bb);	\
	}while(0)

static inline int vm_div( Script * S, TValue * a, TValue * b, TValue * result )
{
	double aa = obj2fnum(S, a, "div");
	double bb = obj2fnum(S, b, "div");

	if(unlikely(bb == 0)) {
		VMError(S, "Divide by Zero.\n");
	}

	setfloatvalue(result, aa / bb);

	return 0;
}

static inline int vm_remainder( Script * S, TValue * a, TValue * b, TValue * result )
{
	double aa = obj2fnum(S,a, "remainder");
	double bb = obj2fnum(S,b, "remainder");


	int ia = (int)aa;
	int ib = (int)bb;

	if(ib <= 0) {
		ib = 1;
	}

	setnumvalue(result, ia % ib);

	return 0;
}

static inline int vm_and( Script * S, TValue * a, TValue * b )
{
	if(ttisnil(a) || ttisnil(b)) {
		return 0;
	}

	if(ttisbool(a) && bvalue(a) == 0) {
		return 0;
	}

	if(ttisbool(b) && bvalue(b) == 0) {
		return 0;
	}

	return 1;
}

static inline int vm_or( Script * S, TValue * a, TValue * b )
{
	if(!ttisnil(a) && !ttisbool(a)) {
		return 1;
	}
	if(!ttisnil(b) && !ttisbool(b)) {
		return 1;
	}

	if(ttisbool(a) && bvalue(a) != 0) {
		return 1;
	}

	if(ttisbool(b) && bvalue(b) != 0) {
		return 1;
	}

	return 0;
}

static inline int vm_not( Script * S, int ra )
{
	TValue * a = stack_n(S, ra);

	if(ttisnil(a) || ttisfalse(a)) {
		return 1;
	} else {
		return 0;
	}
}

static inline int vm_len( Script * S, int ra )
{
	TValue * a = stack_n(S, ra);

	if(ttistbl(a)) {
		int len = rbtH_count(S->r, tblvalue(a));
		return len;
	}

	if(ttisstr(a)) {
		int len = rbtS_len(strvalue(a));
		return len;
	}

	return 1;
}

#define vm_concatenate_quick(S, a, b, c)	\
	do {	\
		const TString * ts_a = rbtO_rawToString(S->r, a);	\
		const TString * ts_b = rbtO_rawToString(S->r, b);	\
		const TString * ts_c = rbtS_concatenate(S->r, ts_a, ts_b);	\
		setstrvalue(c, ts_c);	\
	}while(0)

/*
static inline int vm_concatenate( Script * S, TValue * a, TValue * b, TValue * c )
{
//	TValue * a = stack_n(S, ra);
//	TValue * b = stack_n(S, rb);
//	TValue * c = stack_n(S, rc);

	const TString * ts_a = rbtO_rawToString(S->r, a);
	const TString * ts_b = rbtO_rawToString(S->r, b);

	const TString * ts_c = rbtS_concatenate(S->r, ts_a, ts_b);

	setstrvalue(c, ts_c);

	return 0;
}*/

static const int LogicalBopResult[6][3] = {
	{0, 0, 1},	// GT
	{1, 0, 0},	// LT
	{0, 1, 1},	// GE
	{1, 1, 0},	// LE
	{0, 1, 0},	// EQ
	{1, 0, 1}	// NE
};

#define vm_cmp_quick(a, b, op, result)	\
	do {	\
		if(likely(ttisnumber(a) && ttisnumber(b))) {	\
			double ret = numbervalue(a) - numbervalue(b);	\
			int i = sgn(ret) + 1;	\
			result = LogicalBopResult[op - GT][i];	\
			break;	\
		}	\
		if(likely(ttisstr(a) && ttisstr(b))) {	\
			if(strvalue(a) == strvalue(b)) {	\
				result = LogicalBopResult[op - GT][1];	\
				break;	\
			}	\
			if(likely(op == EQ)) {	\
				result = 0;	\
				break;	\
			}	\
			int ret = strcmp(rbtS_gets(strvalue(a)), rbtS_gets(strvalue(b)));	\
			int i = sgn(ret) + 1;	\
			result = LogicalBopResult[op - GT][i];	\
			break;	\
		}	\
		if(unlikely(ttisnil(a) || ttisnil(b))) {	\
			VMWarn(S, "Null 参与判断！恒为false!\n");	\
			result = 0;	\
			break;	\
		}	\
		int ret = rbtO_rawcmp(a, b);	\
		int i = sgn(ret) + 1;	\
		result = LogicalBopResult[op - GT][i];	\
	}while(0)

static inline int vm_cmp( Script * S, TValue * a, TValue * b, int op )
{
	if( unlikely(ttisnil(a) || ttisnil(b)) ) {

		VMWarn(S, "Null 参与判断！恒为false!\n");
		return 0;
	}

	double ret = rbtO_rawcmp(a, b);
	int i = sgn(ret) + 1;	/* 0, 1, 2 */


	return LogicalBopResult[op - GT][i];
}

static inline int vm_value( Script * S, Table * env, int ra )
{
	TValue * tv = stack_n(S, ra);

	if(!ttisstr(tv)) {
		VMError(S, "Value : Not String.\n");
		return -1;
	}

	const TValue * v = rbtH_getstr(S->r, env, rbtS_gets(strvalue(tv)));
	setvalue_nomark(tv, v);

	return 0;
}

static inline TValue * vm_get_global( Script * S,  Closure * cl, TValue * key )
{
	return rbtH_set(S->r, cl->u.p->env, key);
}

int vm_execute( Script * S )
{
	Closure * cl, *cl2;
	TValue * tv;
	TValue * key;
	const TString * self = rbtS_new(S->r, "self");

	if(!S->ctx || !S->ctx->cl) {
		VMWarn(S, "VM Execute 缺少运行时环境.\n");
		return -1;
	}
	
	Context * ctx = S->ctx;

	cl = ctx->cl;

	if(cl->isC) {
		if(cl->is_yield) {
			// C 函数异步调用结束，统计
			stat_closure_end(cl);
			cl = cl->caller;
			ctx->cl = cl;
			stat_closure_resume(cl);
		} else {
			return cl->u.cf(S);
		}
	}

	if(!cl->u.p->env) {
		cl->u.p->env = S->global;
	}

	Instruction * pi;
	int pc, base;

reentry:

	pc = cl->pc;
	base = cl->base;

	// 首先分配256栈空间，在一个函数里，不可能访问超出127栈空间的
	stack_n(S, base + 256);
	TValue * k_or_reg[2] = {ctx->stack + base, cl->u.p->k};

	while( 1 ) {
begin:
		pi = getni(cl->u.p, pc);
		int op = GetOP(*pi);
		int ka, kb, kc;
		int ra,rb,rc;
		double result;
		TValue *tv_ra, *tv_rb, *tv_rc;

		switch( op ) {
			case MOV:
				ra = GetA(*pi) + base;
				rb = GetB(*pi) + base;
				setvalue_nomark(stack_at(ctx, ra),stack_at(ctx, rb));
				break;

			case SETNIL:
				ra = GetA(*pi) + base;
				rb = GetB(*pi);
				set_nil(S, ra, rb);
				break;

			case SETFALSE:
				ra = GetA(*pi) + base;
				setboolvalue(stack_at(ctx, ra), 0);
				break;

			case SETTRUE:
				ra = GetA(*pi) + base;
				setboolvalue(stack_at(ctx, ra), 1);
				break;

			case LOADK:
				ra = GetA(*pi) + base;
				rb = GetB(*pi);
				tv = &cl->u.p->k[rb];

				setvalue_nomark(stack_at(ctx, ra), tv);
				break;

			case ADD:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_add_quick(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case MINUS:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_minus_quick(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case MULTI:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_multi(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case DIV:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_div_quick(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case REMAINDER:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_remainder(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case AND:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				result = vm_and(S, tv_ra, tv_rb);
				setboolvalue(stack_at(ctx, rc), result);
				break;

			case OR:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				result = vm_or(S, tv_ra, tv_rb);
				setboolvalue(stack_at(ctx, rc), result);
				break;

			case CONCATENATE:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				if(likely(ttisstr(tv_ra) && ttisstr(tv_rb))) {
				//	vm_concatenate_str_str(S, strvalue(tv_ra), strvalue(tv_rb), stack_at(ctx, rc));
					vm_concatenate_str_str_fn(S->r, strvalue(tv_ra), strvalue(tv_rb), stack_at(ctx, rc));
					break;
				}

				if(likely(ttisstr(tv_ra) && ttisnumber(tv_rb))) {
					vm_concatenate_str_num_fn(S->r, strvalue(tv_ra), tv_rb, stack_at(ctx, rc), 0);
					break;
				}
				if(likely(ttisnumber(tv_ra) && ttisstr(tv_rb))) {
					vm_concatenate_str_num_fn(S->r, strvalue(tv_rb), tv_ra, stack_at(ctx, rc), 1);
					break;
				}

				vm_concatenate_quick(S, tv_ra, tv_rb, stack_at(ctx, rc));
				break;

			case GT:
			case LT:
			case GE:
			case LE:
			case EQ:
			case NE:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = GetC(*pi) + base;

				vm_cmp_quick(tv_ra, tv_rb, op, result);
				setboolvalue(stack_at(ctx, rc),cast(int,result));
				break;

			case JGT:
			case JLT:
			case JGE:
			case JLE:
			case JEQ:
			case JNE:
				ra = GetA(*pi);
				rb = GetB(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				rc = cast(signed short int, GetC(*pi));

				vm_cmp_quick(tv_ra, tv_rb, op + GT - JGT, result);

				if((result)) {
					pc += 1 + rc;
					goto begin;
				}
				break;

			case NOT:
				ra = GetA(*pi) + base;
				result = vm_not(S,ra);
				setboolvalue(stack_at(ctx, ra),cast(int,result));
				break;

			case LEN:
				ra = GetA(*pi) + base;
				result = vm_len(S, ra);
				setnumvalue(stack_at(ctx, ra),cast(int, result));
				break;

			case JMP:
				rb = cast(signed short int,GetBx(*pi));
				if(likely(rb != 0)) {	// 不然是没用的跳转,NOP
					pc += rb;
					goto begin;
				}

				break;

			case JTRUE:
				ra = GetA(*pi) + base;
				rb = cast(signed short int,GetBx(*pi));
				tv = stack_at(ctx, ra);
				if(ttistrue(tv)) {
					pc += 1 + rb;
					goto begin;
				}

				break;

			case JFALSE:
				ra = GetA(*pi) + base;
				rb = cast(signed short int,GetBx(*pi));
				tv = stack_at(ctx, ra);
				if(ttisfalse(tv)) {
					pc += 1 + rb;
					goto begin;
				}

				break;

			case GETGLOBAL:
				rb = GetB(*pi);
				rc = GetC(*pi) + base;

				key = &cl->u.p->k[rb];
				if(ttisstr(key) && strvalue(key) == self) {
					// 在closure里面取self
					if(cl->self) {
						settblvalue(stack_at(ctx,rc), cl->self);
					} else {
						setnilvalue(stack_at(ctx,rc));
					}
					break;
				}
				tv = cast(TValue *, rbtH_get(S->r, cl->u.p->env, key));
				if(ttisnil(tv)) {
					tv = cast(TValue*, rbtH_get(S->r, S->lib, key));
				}
				setvalue_nomark(stack_at(ctx, rc),tv);
				break;

			case SETGLOBAL:
				rb = GetB(*pi);
				rc = GetC(*pi) + base;

				key = &cl->u.p->k[rb];

				tv = rbtH_set(S->r, cl->u.p->env, key);
				setvalue(tv,stack_at(ctx, rc));
				break;

			case GETTABLE:
				ra = GetA(*pi);// + base;
				rb = GetB(*pi);// + base;
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				rc = GetC(*pi) + base;

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];

				if(unlikely(!ttistbl(tv_ra))) {
					debug_tvalue_dump(tv_rb);
					debug_tvalue_dump(tv_ra);
				//	VMError(S, "Access(read) Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					VMWarn(S, "Access(read) Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					setnilvalue(stack_at(ctx, rc));
					break;
				}

				const TValue * tmp = NULL;
				if(likely(ttisstr(tv_rb))) {
					vmH_find_str(tmp, S->r, tblvalue(tv_ra), strvalue(tv_rb), 0);
				} else
				if(likely(ttisnum(tv_rb))) {
					vmH_find_num(tmp, S->r, tblvalue(tv_ra), numvalue(tv_rb), 0);
				} else
				if(likely(ttisfnum(tv_rb))) {
					double d = fnumvalue(tv_rb);
					if((double)(int)d == d) {
						vmH_find_num(tmp, S->r, tblvalue(tv_ra), (int)d, 0);
					} else { 
						vmH_find_fnum(tmp, S->r, tblvalue(tv_ra), d, 0);
					}
				} else {
					debug_tvalue_dump(tv_rb);
					VMWarn(S, "Access(read) Table. Invalid Key. Function(%s)\n", rbtS_gets(cl->u.p->fname));
				}

				if(tmp && ttisclosure(tmp)) {
					closurevalue(tmp)->self = tblvalue(tv_ra);
				}
				if(!tmp) {
					setnilvalue(stack_at(ctx, rc));
				} else {
					setvalue_nomark(stack_at(ctx, rc), tmp);
				}
				break;


			case SETTABLE:
				ra = GetA(*pi);// + base;
				rb = GetB(*pi);// + base;
				rc = GetC(*pi);
				ka = GetKa(*pi);
				kb = GetKb(*pi);
				kc = GetKc(*pi);

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];
				tv_rc = &k_or_reg[kc][rc];

				if(unlikely(!ttistbl(tv_ra))) {
					debug_tvalue_dump(tv_rb);
				//	VMError(S, "Acess(write) Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					VMWarn(S, "Acess(write) Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					break;
				}

				if(likely(ttisstr(tv_rb))) {
					TValue * tmp;
					vmH_find_str(tmp, S->r, tblvalue(tv_ra), strvalue(tv_rb), 1);
					setvalue_nomark(tmp, tv_rc);
					break;
				}
				if(likely(ttisnum(tv_rb))) {
					TValue * tmp;
					vmH_find_num(tmp, S->r, tblvalue(tv_ra), numvalue(tv_rb), 1);
					setvalue_nomark(tmp, tv_rc);
					break;
				}
				if(likely(ttisfnum(tv_rb))) {
					double d = fnumvalue(tv_rb);
					TValue * tmp;
					if((double)(int)d == d) {
						vmH_find_num(tmp, S->r, tblvalue(tv_ra), (int)d, 1);
					} else {
						vmH_find_fnum(tmp, S->r, tblvalue(tv_ra), d, 1);
					}
					setvalue_nomark(tmp, tv_rc);
					break;
				}

			//	VMError(S, "Acess(write) Table. Key is not-valid. Function(%s)\n", rbtS_gets(cl->u.p->fname));
				VMWarn(S, "Acess(write) Table. Key is not-valid. Function(%s)\n", rbtS_gets(cl->u.p->fname));
				debug_tvalue_dump(tv_rb);
				break;

			case SETTABLE_NIL:
				ra = GetA(*pi);// + base;
				rb = GetB(*pi);// + base;
				ka = GetKa(*pi);
				kb = GetKb(*pi);

				rc = GetC(*pi) + base;

				tv_ra = &k_or_reg[ka][ra];
				tv_rb = &k_or_reg[kb][rb];

			//	tv = stack_at(ctx, ra);
				if(unlikely(!ttistbl(tv_ra))) {
				//	VMError(S, "Acess Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					VMWarn(S, "Acess Table. But it is not a table. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					break;
				}

				setnilvalue(rbtH_set(S->r, tblvalue(tv_ra), tv_rb));
				break;

			case SETTABLE_NEXTNUM:
				ra = GetA(*pi);// + base;
				ka = GetKa(*pi);

				rc = GetC(*pi) + base;

				tv_ra = &k_or_reg[ka][ra];
			//	tv = stack_at(ctx, ra);
				if(unlikely(!ttistbl(tv_ra))) {
					debug_tvalue_dump(tv_ra);
				//	VMError(S, "Acess Table. But it is not a Table(set table next num). Function(%s)\n", rbtS_gets(cl->u.p->fname));
					VMWarn(S, "Acess Table. But it is not a Table(set table next num). Function(%s)\n", rbtS_gets(cl->u.p->fname));
					break;
				}

				setvalue(rbtH_setnextnum(S->r, tblvalue(tv_ra)), stack_at(ctx, rc));
				break;

			case CALL:
				ra = GetA(*pi) + base;

				tv = stack_at(ctx, ra);
				if(unlikely(!ttisclosure(tv))) {
					VMWarn(S, "Call. It is not a Function. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					break;
				}

				cl2 = rbtScript_closure_dup(closurevalue(tv));
				cl2->self = closurevalue(tv)->self;
				setclosurevalue(tv, cl2);

				cl2->base = ra + 1;
				cl2->caller = cl;

				ctx->cl = cl2;

				stat_closure_pause(cl);

				if(cl2->isC) {
					if(cl2->u.cf(S) == VM_RESULT_YIELD) {
						cl2->is_yield = 1;
						cl->pc = pc + 1;
						return 0;
					}
					cl2->caller = NULL;
					ctx->cl = cl;

					stat_closure_end(cl2);
					stat_closure_resume(cl);

				} else {
					cl->pc = pc + 1;
					cl = cl2;
					ctx->cl = cl;
					goto reentry;
				}

				break;

			case INIT_CALL:
				ra = GetA(*pi) + base;

				tv = stack_at(ctx, ra);
				if(unlikely(!ttisclosure(tv))) {
				//	VMError(S, "Call. It is not a Function. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					VMWarn(S, "Call. It is not a Function. Function(%s)\n", rbtS_gets(cl->u.p->fname));
					break;
				}

				cl2 = closurevalue(tv);
				int nparam = 5;
				if(!cl2->isC) {
					nparam = cl2->u.p->nparam;
				}

				int k;
				for(k = 0; k < nparam; ++k) {
					setnilvalue(stack_at(ctx, ra + 1 + k));
				}

				break;


			case CLOSURE: 
			case CLOSURE_GLOBAL:
				{
					ra = GetA(*pi);
					rb = GetB(*pi) + base;

					if(unlikely(ra >= cl->u.p->sizep)) {
					//	VMError(S, "New Closure(%d). Illegal Access\n", ra);
						VMWarn(S, "New Closure(%d). Illegal Access\n", ra);
						break;
					}

					Closure * cl_new = rbtScript_closure( S );
					// 全局函数不会被销毁，故可以为stable
					if(op == CLOSURE_GLOBAL) {
						rbtC_stable(cast(GCObject*, cl_new));
						cl_new->gc_traverse = NULL;
					}
					cl_new->isC = 0;
					cl_new->u.p = cl->u.p->p[ra];
					if(!cl_new->u.p->env) {
						cl_new->u.p->env = cl->u.p->env;
					}

					setclosurevalue(stack_at(ctx, rb), cl_new);
				}

				break;

			case RETURN:
				ra = GetA(*pi) + base;
				rb = GetB(*pi);

				while(rb--) {
					setvalue_nomark(stack_at(ctx, base), stack_at(ctx, ra));
					base++;
					ra++;
				}

				if(cl->caller) {

					stat_closure_end(cl);

					struct Closure * saved_cl = cl;

					cl = cl->caller;
					ctx->cl = cl;

					stat_closure_resume(cl);

					// 一个closure调用完成，可以直接回收
					{
						S->r->obj--;
						list_del(&saved_cl->gclist);
						if(saved_cl->gc_release) {
							saved_cl->gc_release(cast(GCObject *, saved_cl));
						}
					}
					goto reentry;
				} else {
					goto label_end;
				}

				break;

			case NEWTABLE:
				{
					rc = GetC(*pi) + base;

					Table * tbl = rbtH_init(S->r, 1, 1);
					settblvalue(stack_at(ctx, rc), tbl);
				}

				break;

			case VALUE:
				{
					ra = GetA(*pi) + base;

					vm_value(S, cl->u.p->env, ra);
				}

				break;

			case FOREACH_BEGIN:
				{
					ra = GetA(*pi) + base;

					setnumvalue(stack_at(ctx, ra), -1);
				}

				break;

			case FOREACH:
				{
					ra = GetA(*pi) + base;
					rb = GetB(*pi) + base;
					rc = GetC(*pi) + base;

					TValue * tv = stack_at(ctx, ra);
					if(unlikely(!ttistbl(tv))) {
					//	VMError(S, "Foreach. Expect Table\n");
						VMWarn(S, "Foreach. Expect Table\n");
						setnilvalue(stack_at(ctx, rb));
						setnilvalue(stack_at(ctx, rc));
						break;
					}
					Table * tbl = tblvalue(tv);

					if(unlikely(ra < 1)) {
					//	VMError(S, "Foreach. Idx is Missing\n");
						VMWarn(S, "Foreach. Idx is Missing\n");
						setnilvalue(stack_at(ctx, rb));
						setnilvalue(stack_at(ctx, rc));
						break;
					}

					tv = stack_at(ctx, ra - 1);
					if(unlikely(!ttisnumber(tv))) {
					//	VMError(S, "Foreach. Idx is Not A Number\n");
						VMWarn(S, "Foreach. Idx is Not A Number\n");
						setnilvalue(stack_at(ctx, rb));
						setnilvalue(stack_at(ctx, rc));
						break;
					}

					int idx = numbervalue(tv);

					TValue key, val;

					idx = rbtH_next(S->r, tbl, idx, &key, &val);

					setnumvalue(stack_at(ctx, ra-1), idx);

					if(idx < 0) {
						setnilvalue(stack_at(ctx, rb));
						setnilvalue(stack_at(ctx, rc));
					} else {
						setvalue_nomark(stack_at(ctx, rb), &key);
						setvalue_nomark(stack_at(ctx, rc), &val);
					}
				}

				break;

			case FOREACH_NOKEY:
				{
					ra = GetA(*pi) + base;
					rc = GetC(*pi) + base;

					TValue * tv = stack_at(ctx, ra);
					if(unlikely(!ttistbl(tv))) {
					//	VMError(S, "Foreach. Expect Table\n");
						VMWarn(S, "Foreach. Expect Table\n");
						setnilvalue(stack_at(ctx, rc));
						break;
					}

					Table * tbl = tblvalue(tv);

					tv = stack_at(ctx, ra - 1);
					if(unlikely(!ttisnumber(tv))) {
					//	VMError(S, "Foreach. Idx is Not A Number\n");
						VMWarn(S, "Foreach. Idx is Not A Number\n");
						setnilvalue(stack_at(ctx, rc));
						break;
					}

					int idx = numbervalue(tv);

					TValue val;
					idx = rbtH_next(S->r, tbl, idx, NULL, &val);

					setnumvalue(stack_at(ctx, ra-1), idx);

					if(idx < 0) {
						setnilvalue(stack_at(ctx, rc));
					} else {
						setvalue_nomark(stack_at(ctx, rc), &val);
					}
				}

				break;

			default:
				VMError(S, "Unknown Opcode:%d(%s). PC(%d)\n",op, OpCode_Str[op], pc);
				break;

		}
		pc++;
		cl->pc = pc;
		if(unlikely(pc >= cl->u.p->sizei)) {
			if(cl->caller) {

				stat_closure_end(cl);

				struct Closure * saved_cl = cl;
				cl = cl->caller;
				ctx->cl = cl;

				stat_closure_resume(cl);

				// 一个closure调用完成，可以直接回收
				{
					S->r->obj--;
					list_del(&saved_cl->gclist);
					if(saved_cl->gc_release) {
						saved_cl->gc_release(cast(GCObject *, saved_cl));
					}
				}
				goto reentry;
			} else {
				goto label_end;
			}
		}
	}

label_end:

	if(ctx->f) {
		struct ExtParam * ep = rbtPool_at(S->ext_params, ctx->param);
		ctx->f(S->r, ep, stack_n(S, cl->base));
		if(ep) {
			rbtPool_free(S->ext_params, ep->id);
		}
	}

	// 一个context调用完，可以直接回收
	{
		S->r->obj--;
		list_del(&ctx->gclist);
		if(ctx->cl) {

			stat_closure_end(ctx->cl);

			S->r->obj--;
			list_del(&ctx->cl->gclist);
			if(ctx->cl->gc_release) {
				ctx->cl->gc_release(cast(GCObject*, ctx->cl));
			}
		}

		if(ctx->gc_release) {
			ctx->gc_release(cast(GCObject*, ctx));
		}
	}

	S->ctx = NULL;

	return 0;
}

