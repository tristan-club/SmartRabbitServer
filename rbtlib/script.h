#ifndef script_h_
#define script_h_

#include "script_struct.h"

#define VM_RESULT_OK		0
#define VM_RESULT_YIELD		1
#define VM_RESULT_RESUME	2

struct ExtParam {
	int id;

	int pid;
	int gid;
	short int req_id;

	TString * name;

	GCObject * obj;

	void * p;
};

/*
 *	得到一个 Execute Param，是个local参数，当调用 rbtScript_call 时，
 *
 *	将其 id 传入 param，rbtScript_call 执行结束时回调，会将此参数作为回调参数
 *
 *	@param S
 */
struct ExtParam * rbtScript_ext_param( Script * S );


/*
 *	初始化一个 Script
 *
 *	@param r
 */
Script * rbtScript_init( rabbit * r );

/*
 *	获得一个新的 Closure
 *
 *	@param S
 */
Closure * rbtScript_closure( Script * S );

int rbtScript_nclosure();

/*
 *	复制一个 Closure
 *
 *	@param cl
 */
Closure * rbtScript_closure_dup( Closure * cl );


/*
 *	获得栈的第n个位置
 *
 *	@param S
 *	@param n
 */
inline TValue * stack_n( Script * S, int n );

//#define rbtScript_top(S, n) stack_n(S, S->ctx->cl->base + n)
TValue * rbtScript_top(Script * S, int n);


/*
 *	编译一个脚本文件 or 文件夹
 *
 *	@param S
 *	@param st
 */
int rbtScript_parse( Script * S, const char * path );


/*
 *	注册一个全局函数（库函数）
 *
 *	@param S
 *	@param fname	-- 注册的函数名
 *	@param fun	-- 注册的函数指针，类型为：int(*fun)(Script* S);
 */
void rbtScript_register( Script * S, const char * fname, void * fun);

/*
 *	执行一个脚本调用
 *
 *	@param S
 *	@param fun	-- 回调函数，类型为：void(*fun)(rabbit * r, struct ExtParam * ep, TValue * result);
 *	@param param	-- 回调参数
 *	@param fname	-- 要调用的函数名
 *	@param fmt	-- 参数格式
 *	@param ...	-- 参数
 */
void rbtScript_call( Script * S, void * fun, int param, const char * fname, const char * fmt, ... );
void rbtScript_call_tv( Script * S, void * fun, int param, const TValue * tv, const char * fmt, ... );

void rbtScript_get( Script * S, const char * fname, TValue * result );


/*
 *	保持当前运行时环境
 *
 *	@param S
 */
Context * rbtScript_save( Script * S );


/*
 *	恢复一个运行时环境
 *
 *	@param S
 *	@param ctx
 */
void rbtScript_resume( Script * S, Context * ctx );


/*
 *	继续执行一个运行时环境
 *
 *	@param S
 */
void rbtScript_run( Script * S );


/*
 *	给 Script 加入扩充(eXtention)
 *
 *	@param S
 *	@param x
 */
void rbtScript_x( Script * S, struct ScriptX * x );

void rbtScript_set_d( Script * S, void * p );
void * rbtScript_get_d( Script * S );

/*
 *	取得 Script 的扩充
 *
 *	@param S
 */
struct ScriptX * rbtScript_get_x( Script * S );

/*
 *	编译、执行出错时，跳到最初的位置
 *
 *	@param S
 *	@param fmt	-- 出错信息格式
 *	@param ...	-- 出错信息
 */
void die( Script * S, const char * fmt, ... );

/*
 *	在程序中可以调用rbtScript_die，会输出在脚本的哪一行die的
 *
 */
void rbtScript_die(Script * S);

/*
 *
 *	在程序中可以调用rbtScript_warn，打印Warning
 *
 */
void rbtScript_warn(Script * S, const char * msg);


/*
 *	输出一个脚本的信息
 *
 *	@param S
 */
void rbtD_script( Script * S );

int rbtD_ncontext();
int rbtD_mcontext();

#endif

