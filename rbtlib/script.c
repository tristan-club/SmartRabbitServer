#include "script.h"
#include "script_load.h"
#include "mem.h"
#include "gc.h"
#include "code.h"
#include "math.h"

#include "rabbit.h"
#include "object.h"

#include "table.h"
#include "string.h"
#include "pool.h"

#include "vm.h"

#include "script_base_lib.h"

#include "stream.h"

#define MAX_CONTEXT_COUNT	5000
#define MAX_CLOSURE_COUNT	50000

static int nctx = 0;
static int mctx = 0;

inline TValue * stack_n( Script * S, int i )
{
	if(!S->ctx) {
		return NULL;
	}

	if(unlikely(S->ctx->ssize <= i)) {
		S->ctx->stack = RREALLOC(S->r, TValue, S->ctx->stack, S->ctx->ssize, i + 32);

		mctx += (i + 32 - S->ctx->ssize) * sizeof(TValue);

		int j;
		for(j = S->ctx->ssize; j < i + 32; ++j) {
			setnilvalue(&S->ctx->stack[j]);
		}

		S->ctx->ssize = i + 32;
	}

	return &(S->ctx->stack[i]);
}

TValue * rbtScript_top(Script * S, int n)
{
	static TValue tv;

	if(S->ctx && S->ctx->cl) {
		return stack_n(S, S->ctx->cl->base + n);
	}

	kLOG(S->r, 0, "[Error] %s : S->ctx(%p)有误！\n", __FUNCTION__, S->ctx);

	return &tv;
}

static void ctx_traverse( GCObject * obj )
{
	Context * ctx = cast(Context *, obj);

	if(ctx->S) {
		rbtC_mark(cast(GCObject *, ctx->S));
	}

	if(ctx->x) {
		rbtC_mark(ctx->x);
	}

	if(ctx->cl) {
		rbtC_mark(ctx->cl);
	}

	if(ctx->this) {
		rbtC_mark(ctx->this);
	}

	if(ctx->stat_name) {
		rbtC_mark(cast(GCObject*, ctx->stat_name));
	}

	int i;
	for(i = 0;i < ctx->ssize; ++i) {
		TValue * tv = &ctx->stack[i];
		if(is_collectable(tv)) {
			rbtC_mark(gcvalue(tv));
		}
	}
}

static void ctx_release( GCObject * obj )
{
	Context * ctx = cast(Context *, obj);

//	if(ctx->cl) {
		// Context 从get_ctx到ctx_release经过的时间，此时的ctx->cl应该是ctx启动时第一个closure
		stat_context_end(ctx);
//	}

	ctx->cl = NULL;
	ctx->f = NULL;
	ctx->param = 0xDEADBEEF;
	ctx->x = NULL;

	ctx->this = NULL;

	if(nctx > MAX_CONTEXT_COUNT) {

		nctx--;
		mctx -= ctx->ssize * sizeof(TValue);
		mctx -= sizeof(Context);

		RFREEVECTOR(ctx->r, ctx->stack, ctx->ssize);
		RFREE(ctx->r, ctx);
		return;
	}

	if(ctx->ssize > Max_Stack_Size) {
		kLOG(NULL, 0, "[Warnning]Huge Context Stack!\n");
		mctx -= ctx->ssize * sizeof(TValue);
		RFREEVECTOR(ctx->r, ctx->stack, ctx->ssize);
		ctx->stack = NULL;
		ctx->ssize = 0;
	} else {
		int i;
		for(i = 0;i < ctx->ssize; ++i) {
			TValue * tv = &ctx->stack[i];
			setnilvalue(tv);
		}
	}

	list_insert(&ctx->S->context_list, &ctx->list);
}

static Context * get_ctx( Script * S )
{
	Context * ctx;

	if(list_empty(&S->context_list)) {

		ctx = RMALLOC(S->r, Context, 1);

		ctx->stack = NULL;
		ctx->ssize = 0;

		nctx++;
		mctx += sizeof(Context);
	} else {

		struct list_head * elem = list_first_entry(&S->context_list);

		ctx = list_entry(elem, struct Context, list);

		list_del(elem);
	}

	rbtC_link(S->r, cast(GCObject *, ctx), TCONTEXT);

	ctx->gc_traverse = ctx_traverse;
	ctx->gc_release = ctx_release;

	list_init(&ctx->list);

	ctx->S = S;
	ctx->cl = NULL;
	ctx->f = NULL;
	ctx->param = 0xDEADBEEF;

	ctx->x = NULL;
	ctx->void_x = NULL;

	ctx->this = NULL;

	gettimeofday(&ctx->start_tm, NULL);
	ctx->stat_name = NULL;

	return ctx;
}

int rbtM_context(struct Context * ctx)
{
	int m = sizeof(struct Context);
	m += sizeof(TValue) * ctx->ssize;

	return m;
}

int rbtD_ncontext()
{
	return nctx;
}
int rbtD_mcontext()
{
	return mctx;
}

static void run( Script * S, Closure * cl )
{
	Context * ctx = get_ctx( S );

	S->ctx = ctx;

	setclosurevalue(stack_n(S, 0), cl);

	cl->base = 1;

	ctx->cl = cl;

	if(!cl->isC && cl->u.p) {
		ctx->stat_name = cl->u.p->fname;
	}

	vm_execute( S );
}

static int is_file_name_numeric( const char * p, int len )
{
	char c = p[0];
	if(c >= '0' && c <= '9') {
		return stou(p, len, NULL);
	}

	return -1;
}

static int is_valid_file_name( Script * S, const char * path, int len )
{
	if(len <= 4) {
		return 0;
	}
	if(S->use_orz) {
		if(path[len-1] != 'z' || path[len-2] != 'r' || path[len-3] != 'o' || path[len-4] != '.') {
			return 0;
		}
		return 1;
	}
	if(path[len-1] != 'o' || path[len-2] != 'r' || path[len-3] != 'z' || path[len-4] != '.') {
		return 0;
	}
	return 1;
}

static void add_closure( Script * S, Proto * p )
{
	Closure * cl = rbtScript_closure( S );
	cl->isC = 0;
	cl->u.p = p;
	cl->pc = 0;

	if(S->closureused >= S->closuresize) {
		S->files = RREALLOC(S->r, Closure *, S->files, S->closuresize, S->closuresize + 4);
		S->closuresize += 4;
	}
	S->files[S->closureused++] = cl;
}

static int do_parse( Script * S, const char * path, const char * fname, Table * env, Table * parent )
{
	struct stat file_stat;
	if(stat(path, &file_stat) < 0) {
		kLOG(NULL, 0, "[Error] do parse : Can't Stat(%s)\n", path);
		return 0;
	}

	if(S_ISREG(file_stat.st_mode)) {
		int len = strlen(path);
		if(len <= 4) {
			return 0;
		}
		if(!is_valid_file_name(S, path, len)) {
			if(fname && parent) {
				rbtH_rmstr(S->r, parent, fname);
			}
			return 0;
		}

		stream * st = stream_open(S->r, path);
		if(!st) {
			kLOG(NULL, 0, "[Error] do Parse(%s). File Cant Open\n", path);
			return 0;
		}

		Proto * p;
		if(S->use_orz) {
			script_parse(S, st, env);
		} else {
			p = script_load(S, st, env);
			add_closure(S, p);
		}

		return 0;
	}

	if(S_ISDIR(file_stat.st_mode)) {
		DIR * dir;
		struct dirent * dp;
		if((dir = opendir(path)) == NULL) {
			kLOG(NULL, 0, "[Error] Parse (%s). Open Dir Error\n", path);
			return 0;
		}

		rbtH_rmstr(S->r, env, "FILENAME");

		while( (dp = readdir(dir)) != NULL) {
			if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, ".svn") == 0) {
				continue;
			}

			Table * env_sec = rbtH_init(S->r, 1, 1);

			const char * p = dp->d_name;
			int len = strlen(p);
			if(len > 4) {
				if(is_valid_file_name(S, p, len)) {
					const TString * ts = rbtS_init_len(S->r, p, len - 4);
					p = rbtS_gets(ts);
				}
			}

			int numeric_fname = is_file_name_numeric(p, len - 4);

			if(numeric_fname < 0) {
				settblvalue(rbtH_setstr(S->r, env, p), env_sec);
			} else {
				settblvalue(rbtH_setnum(S->r, env, numeric_fname), env_sec);
			}
			settblvalue(rbtH_setstr(S->r, env_sec, "Root"), S->global);
			setstrvalue(rbtH_setstr(S->r, env_sec, "FILENAME"), rbtS_new(S->r, p));

			char * file_name = RMALLOC(S->r, char, 1024);
			memset(file_name, 0, 1024);
			snprintf(file_name, 1023, "%s/%s", path, dp->d_name);
			do_parse(S, file_name, p, env_sec, env);
			RFREEVECTOR(S->r, file_name, 1024);
		}

		return 0;
	}

	return 0;
}

int rbtScript_parse( Script * S, const char * path )
{
	S->closureused = 0;

	if(!setjmp(S->long_jump)) {

		do_parse(S, path, NULL, S->global, NULL);

		kLOG(NULL, 0, "[LOG]脚本解析完毕，共有：%d个脚本文件！\n", S->closureused);

		int i;
		for(i = 0; i < S->closureused; ++i) {
			struct Closure * cl = S->files[i];
			gettimeofday(&cl->start_tm, NULL);
			run(S, cl);
		}

		int ncl, ngl;
		code_get_closure(&ncl, &ngl);

		S->r->debug_is_script_end = 1;
		fprintf(stderr, "[LOG]脚本解析完毕，共有：%d个脚本文件, %d个函数，%d个全局函数！\n", S->closureused, ncl, ngl);

		return 0;
	} else {
		return -1;
	}
}

int rbtScript_re_parse( Script * S, const char * path )
{
	S->closureused = 0;
	return 0;
}

void rbtScript_register( Script * S, const char * fname, void * fun )
{
	Closure * cl = rbtScript_closure(S);

	cl->cf_name = rbtS_new(S->r, fname);
	cl->isC = 1;
	cl->u.cf = fun;

	setclosurevalue(rbtH_setstr(S->r, S->lib, fname), cl);
}

static void spt_traverse( GCObject * obj )
{
	Script * spt = cast(Script *, obj);

	if(spt->lib) {
		rbtC_mark(spt->lib);
	}

	if(spt->global) {
		rbtC_mark(spt->global);
	}

	if(spt->ctx) {
		rbtC_mark(cast(GCObject *, spt->ctx));
	}

	Context * ctx;
	struct list_head * elem;

	list_foreach(elem, &spt->context_list) {
		ctx = list_entry(elem, struct Context, list);
		rbtC_mark(cast(GCObject *, ctx));
	}

	Closure * cl;

	list_foreach(elem, &spt->closure_list) {
		cl = list_entry(elem, struct Closure, list);
		rbtC_mark(cl);
	}

	if(spt->x) {
		rbtC_mark(spt->x);
	}

	if(spt->_export) {
		rbtC_mark(spt->_export);
	}
}

static void spt_release( GCObject * obj )
{
	Script * spt = cast(Script *, obj);

	RFREEVECTOR(spt->r, spt->files, spt->closuresize);
}

Script * rbtScript_init( rabbit * r )
{
	Script * S = RMALLOC(r, Script, 1);

	rbtC_link(r, cast(GCObject*, S), TSCRIPT);

	S->gc_traverse = spt_traverse;
	S->gc_release = spt_release;

	S->global = rbtH_init(r, 1, 1);
	S->lib = rbtH_init(r, 1, 1);

	settblvalue(rbtH_setstr(r, S->lib, "Root"), S->global);

	S->use_orz = 1;
	S->generate_zro = 0;

	S->ctx = NULL;

	list_init(&S->context_list);

	list_init(&S->closure_list);

	S->files= NULL;
	S->closuresize = 0;
	S->closureused = 0;

	S->x = NULL;
	S->void_x = NULL;

	S->ext_params = rbtPool_init(r, sizeof(struct ExtParam), 16);

	S->_export = rbtH_init(r, 1, 1);
	rbtH_weak(S->_export);

	rbtScript_lib( S );

	return S;
}

struct ExtParam * rbtScript_ext_param( Script * S )
{
	int id = rbtPool_push(S->ext_params);

	struct ExtParam * ep = rbtPool_at(S->ext_params, id);

	ep->id = id;
	ep->pid = ep->gid = ep->req_id = 0;

	ep->name = NULL;

	ep->obj = NULL;

	ep->p = NULL;

	return ep;
}

static void cl_traverse( GCObject * obj )
{
	Closure * cl = cast(Closure *, obj);

	if(cl->isC) {
		if(cl->cf_name) {
			rbtC_mark(cast(void *, cl->cf_name));
		}
		return;
	}

	if(cl->self) {
		rbtC_mark(cl->self);
	}
}


static int ncl = 0;

static void cl_release( GCObject * obj )
{
	Closure * cl = cast(Closure *, obj);

	if(ncl > MAX_CLOSURE_COUNT) {
		ncl--;

		RFREE(cl->r, cl);
		return;
	}

	struct list_head * head = &cl->S->closure_list;

	list_insert(head, &cl->list);

	cl->u.p = NULL;
	cl->caller = NULL;
	cl->self = NULL;
}

Closure * rbtScript_closure( Script * S )
{
	Closure * cl;

	if(list_empty(&S->closure_list)) {

		cl = RMALLOC(S->r, Closure, 1);

		ncl++;

	} else {

		struct list_head * elem = list_first_entry(&S->closure_list);

		cl = list_entry(elem, Closure, list);

		list_del(elem);
	}

	rbtC_link(S->r, cast(GCObject*, cl), TCLOSURE);

	list_init(&cl->list);

	cl->gc_release = cl_release;
	cl->gc_traverse = cl_traverse;

	cl->S = S;

	cl->isC = 0;
	cl->is_yield = 0;
	cl->u.p = NULL;
	cl->cf_name = NULL;

	cl->pc = 0;
	cl->base = 0;

	cl->self = NULL;

	cl->caller = NULL;

	cl->run_tm = 0;
	gettimeofday(&cl->resume_tm, NULL);
	cl->is_pause = 0;
	cl->start_tm = cl->resume_tm;

	return cl;
}

int rbtScript_nclosure()
{
	return ncl;
}

Closure * rbtScript_closure_dup( Closure * old )
{
	Closure * cl = rbtScript_closure(old->S);

	cl->isC = old->isC;
	cl->u = old->u;
	cl->cf_name = old->cf_name;

	return cl;
}

void die( Script * S, const char * fmt, ... )
{
	static char buf[1024];
	memset(buf, 0, sizeof(buf));

	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, 1023, fmt, va);
	kLOG(S->r, 0, "[Error]%s", buf);
	va_end(va);

	longjmp(S->long_jump, 1);
}

void rbtScript_get( Script * S, const char * path, TValue * val )
{
	setnilvalue(val);

	const TValue * tv;

	Table * tbl = S->global;

	char * Path = strdup(path);
	char * prev = strtok(Path, ".");
	char * next = strtok(NULL, ".");
	while(next) {
		tv = rbtH_getstr(S->r, tbl, prev);
		if(ttistbl(tv)) {
			tbl = tblvalue(tv);
		} else {
			free(Path);
			return;
		}
		prev = next;
		next = strtok(NULL, ".");
	}

	tv = rbtH_getstr(S->r, tbl, prev);

	setvalue(val, tv);

	free(Path);
}

void rbtScript_call_tv( Script * S, void * f, int param, const TValue * tv, const char * fmt, ... )
{
	// 找到要调用的函数
	Table * tbl;

	if(!ttisclosure(tv)) {
		kLOG(NULL, 0, "[Error] Script Call tv(). Not a Function\n");
		return;
	}

	// 新的执行环境
	{
		Closure * cl = rbtScript_closure_dup(closurevalue(tv));
		cl->self = closurevalue(tv)->self;

		Context * ctx = get_ctx(S);

		S->ctx = ctx;

		setclosurevalue(stack_n(S, 0), cl);
		ctx->cl = cl;
		cl->base = 1;

		ctx->f = f;
		ctx->param = param;
		ctx->x = S->x;
		ctx->void_x = S->void_x;

		S->x = NULL;
		S->void_x = NULL;

		if(!cl->isC && cl->u.p) {
			ctx->stat_name = cl->u.p->fname;
		}
	}

	if(!fmt) {
		goto label_call;
	}

	// 将参数依次压入栈
	va_list va;
	va_start(va, fmt);

	int i;
	double d;
	const TString * ts;
	const char * cstr;
	char * p;

	int top = 1;
	while(*fmt) {
		char c = *fmt++;
		switch(c) {
			case 'b':
				i = va_arg(va, int);
				setboolvalue(stack_n(S, top++), i == 0 ? 0 : 1);
				break;

			case 'd':
				i = va_arg(va, int);
				setnumvalue(stack_n(S, top++), i);
				break;
				
			case 'f':
				d = va_arg(va, double);
				setfnumvalue(stack_n(S, top++), d);
				break;

			case 'S':
				ts = va_arg(va, const TString *);
				setstrvalue(stack_n(S, top++), ts);
				break;

			case 's':
				cstr = va_arg(va, const char *);
				setstrvalue(stack_n(S, top++), rbtS_new(S->r, cstr));
				break;

			case 'h':
				tbl = va_arg(va, Table *);
				settblvalue(stack_n(S, top++), tbl);
				break;

			case 'p':
				p = va_arg(va, char*);
				setpvalue(stack_n(S, top++), p);
				break;

			default:
				kLOG(NULL, 0, "[Error]Script Call tv. Invalid Argument : %c\n", c);
				break;
		}
	}

	// 调用
label_call:

	if(!setjmp(S->long_jump)) {
		vm_execute(S);
	} else {
		if(f) {
			int(*fun)(rabbit *, struct ExtParam *, const TValue *) = f;
			TValue tv;
			setnilvalue(&tv);

			struct ExtParam * ep = rbtPool_at(S->ext_params, param);

			fun(S->r, ep, &tv);

			rbtPool_free(S->ext_params, param);
		}
	}

	return;
}
void rbtScript_call(Script * S, void * f, int param, const char * fname, const char * fmt, ... )
{
	// 找到要调用的函数
	const TValue * tv;

	const TString * ts_fname = rbtS_new(S->r, fname);

	TValue val;
	rbtScript_get(S, fname, &val);
	tv = &val;

	Table * tbl;

	if(!ttisclosure(tv)) {
		kLOG(NULL, 0, "[Error]Script Call(%s). Not a Function\n", fname);
		return;
	}

	// 新的执行环境
	{
		Closure * cl = rbtScript_closure_dup(closurevalue(tv));

		Context * ctx = get_ctx(S);

		ctx->stat_name = ts_fname;

		S->ctx = ctx;

		setclosurevalue(stack_n(S, 0), cl);
		ctx->cl = cl;
		cl->base = 1;

		ctx->f = f;
		ctx->param = param;
		ctx->x = S->x;
		ctx->void_x = S->void_x;
		S->x = NULL;
		S->void_x = NULL;
	}

	if(!fmt) {
		goto label_call;
	}

	// 将参数依次压入栈
	va_list va;
	va_start(va, fmt);

	int i;
	double d;
	const TString * ts;
	const char * cstr;
	char * p;
	TValue * t;

	int top = 1;
	while(*fmt) {
		char c = *fmt++;
		switch(c) {
			case 'b':
				i = va_arg(va, int);
				setboolvalue(stack_n(S, top++), i == 0 ? 0 : 1);
				break;

			case 'd':
				i = va_arg(va, int);
				setnumvalue(stack_n(S, top++), i);
				break;
				
			case 'f':
				d = va_arg(va, double);
				setfnumvalue(stack_n(S, top++), d);
				break;

			case 'S':
				ts = va_arg(va, const TString *);
				setstrvalue(stack_n(S, top++), ts);
				break;

			case 's':
				cstr = va_arg(va, const char *);
				setstrvalue(stack_n(S, top++), rbtS_new(S->r, cstr));
				break;

			case 'h':
				tbl = va_arg(va, Table *);
				settblvalue(stack_n(S, top++), tbl);
				break;

			case 'p':
				p = va_arg(va, char*);
				setpvalue(stack_n(S, top++), p);
				break;

			case 't':
				t = va_arg(va, TValue *);
				setvalue(stack_n(S, top++), t);
				break;

			default:
				kLOG(NULL, 0, "[Error]Script Call(%s). Invalid Argument : %c\n", fname, c);
				break;
		}
	}

	// 调用
label_call:

	if(!setjmp(S->long_jump)) {
		vm_execute(S);
	} else {
		if(f) {
			int(*fun)(rabbit *, struct ExtParam *, const TValue *) = f;
			TValue tv;
			setnilvalue(&tv);

			struct ExtParam * ep = rbtPool_at(S->ext_params, param);

			fun(S->r, ep, &tv);

			rbtPool_free(S->ext_params, param);
		}
	}

	return;
}

inline Context * rbtScript_save( Script * S )
{
	Context * ctx = S->ctx;
	S->ctx = NULL;

	const TValue * tv = rbtH_getstr(S->r, S->lib, "this");
	if(ttistbl(tv)) {
		ctx->this = tblvalue(tv);
		/* 修复this切换时`漏mark'BUG */
		rbtC_mark(ctx->this);
	}
	if(ctx->cl) {
		stat_closure_pause(ctx->cl);
	}

	return ctx;
}

inline void rbtScript_resume( Script * S , Context * ctx )
{
	S->ctx = ctx;

	if(ctx->this) {
		settblvalue(rbtH_setstr(S->r, S->lib, "this"), ctx->this);
	}
	// 1.修复 lib["this"] ctx->this 互转bug
	// ctx->this = NULL;
	if(ctx->cl) {
		stat_closure_resume(ctx->cl);
	}
}

inline void rbtScript_run( Script * S )
{
	if(!setjmp(S->long_jump)) {
		vm_execute(S);
	}
}

inline void rbtScript_x( Script * S, struct ScriptX * x )
{
	S->x = x;
}

inline struct ScriptX * rbtScript_get_x( Script * S )
{
	if(S->ctx) {
		return S->ctx->x;
	}

	return NULL;
}

inline void rbtScript_set_d( Script * S, void * d )
{
	S->void_x = d;
}

inline void * rbtScript_get_d( Script * S )
{
	if(S->ctx) {
		return S->ctx->void_x;
	}

	return NULL;
}

void rbtScript_die( Script * S )
{
	const TString * fname = S->ctx->cl->caller->u.p->fname;
	const TString * file = S->ctx->cl->caller->u.p->file;
	int line = S->ctx->cl->caller->u.p->line[S->ctx->cl->caller->pc] + 1;

	kLOG(NULL, 0, "[Error]Die At(%s:%s:%d)\n", rbtS_gets(file), rbtS_gets(fname), line);
	exit(1);
}

