#ifndef statistic_h_
#define statistic_h_

#include "common.h"

struct rabbit;

/*
 *	统计数据包
 */
struct Statistic {
	int npkt_sent;
	int sent_size;

	int npkt_recv;
	int recv_size;

	int mem;
};

void rbtStat_init(struct rabbit * r, struct Statistic * stat);

/*
 *	统计运行时间
 *
 */
struct StatRunTime {
	msec_t start_tm;
	msec_t duration;

	size_t script_count;	// 脚本执行的次数
	size_t script_tm;	// 脚本执行的时间

	Table * count_tbl;
	Table * tm_tbl;

	Table * cl_count_all_tbl;
	Table * cl_tm_all_tbl;

	Table * ctx_count_tbl;
	Table * ctx_tm_tbl;
};

#define STAT_SCRIPT	(1 << 0)
#define STAT_CONTEXT	(1 << 1)
#define STAT_CLOSURE	(0 << 1)
#define STAT_ALL_TIME	(1 << 2)


#ifdef STAT_RUN_TIME

void rbtStat_rt_init(struct rabbit * r, msec_t duration);

void rbtStat_rt_gc(struct rabbit * r);

int rbtStat_rt_update(struct rabbit * r, struct StatRunTime * stat);

void rbtStat_rt_add_ex(struct rabbit * r, const TString * file, const TString * fun, int usec, int flag);

#define rbtStat_rt_start(r, name)	\
	struct timeval _stat_##name;	\
	gettimeofday(& _stat_##name, NULL);

#define rbtStat_rt_end(r, name)	do {	\
	struct timeval now;	\
	gettimeofday(&now, NULL);	\
	const TString * module = rbtS_new(r, #name);	\
	rbtStat_rt_add_ex(r, NULL, module, (now.tv_sec - _stat_##name.tv_sec) * 1000000 + now.tv_usec - _stat_##name.tv_usec, 0);	\
}while(0)

#else	// else not def STAT_RUN_TIME

#define rbtStat_rt_init(r, duration)
#define rbtStat_rt_gc(r)
#define rbtStat_rt_update(r, stat)	0

#define rbtStat_rt_add_ex(r, file, fun, usec, flag)

#define rbtStat_rt_start(r, name)
#define rbtStat_rt_end(r, name)

#endif // STAT_RUN_TIME


#endif

