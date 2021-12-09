#ifndef script_struct_h_
#define script_struct_h_

#include "object.h"
#include "statistic.h"

#include "list.h"

#define Max_Stack_Size	1024

typedef unsigned int Instruction;

typedef struct LocVar LocVar;

struct LocVar {
	TString * name;
	int reg;
};

struct Proto {
//	CommonHeader;

	struct list_head link;	// 所有的Proto链在一起，用于统计信息

	rabbit * r;

	int nparam;		// 函数参数个数

	const TString * file;
	TString * fname;

	Table * h;      // hash to k

	Table * env;    // global table

	TValue * k;     // constant used in the function
	size_t sizek;

	Instruction * i;        // instructions
	int * line;		// instruction 对应的文件行
	size_t sizei;

	LocVar * locvars;       // local variables information
	size_t sizelocvars;

	TString ** upvalues;    // upvalue names
	size_t sizeuv;

	struct Proto ** p;      // function defined in this function
	size_t sizep;

	struct Proto * parent;
};

// 当前系统中共有多少proto，以及所有的proto占用内存大小
int rbtScript_proto_count();
int rbtScript_proto_mem();

struct Closure {
	CommonHeader;

	struct list_head list;

	Script * S;

	int isC;
	int is_yield;

	union {
		int (*cf)( struct Script * S );
		struct Proto * p;
	}u;

	const TString * cf_name;	// c function 注册时的name

	int pc;
	int base;

	Table * self;		// obj.fun()，这种调用里面，fun 内可以引用 self，指的是 obj

	struct Closure * caller;

	/* 统计运行时间 */
	struct timeval resume_tm;
	long long unsigned int run_tm;
	int is_pause;
	struct timeval start_tm;
};


struct ScriptX;
int rbtM_scriptX(struct ScriptX * sptX);

struct ExtParam;

struct Context {
	CommonHeader;

	struct list_head list;

	Script * S;

	TValue * stack;
	size_t ssize;

	Closure * cl;

	void(*f)(rabbit *, struct ExtParam * param, TValue * tv);

	int param;

	Table * this;	// 执行的时候的this，save的时候保存下来，resume的时候赋值上去

	struct ScriptX * x;
	void * void_x;	// 指向 Script->void_x 

	// ---- 统计运行时间 --------
	struct timeval start_tm;
	const TString * stat_name;
};
int rbtM_context(struct Context * ctx);


struct Script {
	CommonHeader;

	jmp_buf  long_jump;

	// 加载文件的时候，是加载.orz文件还是加载.zro文件
	// 其中 .orz 文件 是脚本源文件，.zro文件时脚本编译好的字节码文件
	// 默认是使用 .orz 文件
	int use_orz;
	int generate_zro;	// 是否生成 .zro 文件，默认不生成


	/* global 和 lib 的区别：
	   1、 global 是名字空间，它其中有子名字空间，处于父空间的如果要访问子空间的变量，需要Child.Var形式访问，子空间不能访问父空间的变量
	   2、 lib 是全局的空间，如果脚本在自己的名字空间里没有找到相应的变量，会再次在lib里搜索，子空间、父空间，都能访问同一个lib里的变量
	   3、 在自己名字空间里定义的变量，会覆盖 lib 里的变量，因为VM会先在自己的名字空间里搜索变量，如果找不到，才到lib里去找
	 */
	Table * global;
	Table * lib;

	Context * ctx;	// 正在运行的环境

	struct list_head context_list;	// 空闲 Context 链

	struct list_head closure_list;	// 空闲 Closure 链

	struct Closure ** files;      // 解析出的 Closure
	int closuresize;
	int closureused;

	struct ScriptX * x;	// 用于扩充
	void * void_x;		// 扩充2

	Pool * ext_params;

	Table * _export;	// export(fun_id, closure);
};

#ifdef STAT_RUN_TIME

#define stat_closure_resume(cl)	do {	\
	struct Closure * __cl = cl;	\
	if(__cl->is_pause) {	\
		gettimeofday(&__cl->resume_tm, NULL);	\
	       	__cl->is_pause = 0;	\
	}	\
}while(0)

#define stat_closure_pause(cl)	do {	\
	struct Closure * __cl = cl;	\
	if(!__cl->is_pause) {	\
		struct timeval now;	\
		gettimeofday(&now, NULL);	\
		__cl->run_tm += (now.tv_sec - __cl->resume_tm.tv_sec) * 1000000 + now.tv_usec - __cl->resume_tm.tv_usec;	\
	       	__cl->resume_tm = now;	\
		__cl->is_pause = 1;	\
	}	\
}while(0)

#define stat_closure_end(cl)	do {	\
	struct Closure * __cl = cl; 	\
	if(!__cl) {	\
		break;	\
	}	\
	if(__cl->is_pause) {	\
		break;	\
	}	\
	struct timeval now;	\
	gettimeofday(&now, NULL);	\
	__cl->run_tm += (now.tv_sec - __cl->resume_tm.tv_sec) * 1000000 + now.tv_usec - __cl->resume_tm.tv_usec;	\
	__cl->resume_tm = now;	\
	int all_tm = (now.tv_sec - __cl->start_tm.tv_sec) * 1000000 + now.tv_usec - __cl->start_tm.tv_usec;	\
	const TString * fname = NULL;	\
	const TString * file = NULL;	\
	if(!__cl->isC) {	\
		fname = __cl->u.p->fname;	\
		file = __cl->u.p->file;	\
	} else {	\
		fname = __cl->cf_name;	\
	}	\
	rbtStat_rt_add_ex(__cl->r, file, fname, __cl->run_tm, STAT_SCRIPT | STAT_CLOSURE);	\
	rbtStat_rt_add_ex(__cl->r, file, fname, all_tm, STAT_CLOSURE | STAT_ALL_TIME);	\
}while(0)

#define stat_context_end(ctx)	do {	\
	struct Context * __ctx = ctx; 	\
	if(!__ctx) {	\
		break;	\
	}	\
	struct timeval now;	\
	gettimeofday(&now, NULL);	\
	int val = (now.tv_sec - __ctx->start_tm.tv_sec) * 1000000 + now.tv_usec - __ctx->start_tm.tv_usec;	\
	const TString * fname = __ctx->stat_name;	\
	if(fname) {	\
		rbtStat_rt_add_ex(__ctx->r, NULL, fname, val, STAT_CONTEXT);	\
	}	\
}while(0)

#else	// not def STAT_RUN_TIME

#define stat_closure_resume(cl)
#define stat_closure_pause(cl)
#define stat_closure_end(cl)
#define stat_context_end(ctx)

#endif	// STAT_RUN_TIME

#endif

