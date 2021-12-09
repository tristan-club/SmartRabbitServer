#include "statistic.h"
#include "object.h"
#include "rabbit.h"
#include "table.h"
#include "string.h"
#include "gc.h"

void rbtStat_init(struct rabbit * r, struct Statistic * stat)
{
	stat->npkt_sent = 0;
	stat->sent_size = 0;
	stat->npkt_recv = 0;
	stat->recv_size = 0;

	stat->mem = 0;
}

#ifdef STAT_RUN_TIME

static void stat_rt_restart(struct rabbit * r)
{
	struct StatRunTime * stat = &r->stat_rt;

	stat->start_tm = rbtTime_curr(r);
	stat->script_count = 0;
	stat->script_tm = 0;

	stat->count_tbl = rbtH_init(r, 1, 512);
	stat->tm_tbl = rbtH_init(r, 1, 512);

	stat->cl_count_all_tbl = rbtH_init(r, 1, 512);
	stat->cl_tm_all_tbl = rbtH_init(r, 1, 512);

	stat->ctx_count_tbl = rbtH_init(r, 1, 512);
	stat->ctx_tm_tbl = rbtH_init(r, 1, 512);
}

void rbtStat_rt_init(struct rabbit * r, msec_t duration)
{
	struct StatRunTime * stat = &r->stat_rt;

	memset(stat, 0, sizeof(struct StatRunTime));

	stat->duration = duration;

	if(stat->duration > 0) {
		stat_rt_restart(r);
	}
}

void rbtStat_rt_gc(struct rabbit * r)
{
	struct StatRunTime * stat = &r->stat_rt;
	if(stat->count_tbl){
		rbtC_mark(stat->count_tbl);
	}
	if(stat->tm_tbl) {
		rbtC_mark(stat->tm_tbl);
	}
	if(stat->cl_count_all_tbl) {
		rbtC_mark(stat->cl_count_all_tbl);
	}
	if(stat->cl_tm_all_tbl) {
		rbtC_mark(stat->cl_tm_all_tbl);
	}
	if(stat->ctx_count_tbl) {
		rbtC_mark(stat->ctx_count_tbl);
	}
	if(stat->ctx_tm_tbl) {
		rbtC_mark(stat->ctx_tm_tbl);
	}
}

int rbtStat_rt_update(struct rabbit * r, struct StatRunTime * out)
{
	struct StatRunTime * stat = &r->stat_rt;
	if(!stat->count_tbl) {
		// 没有开启统计
		return 0;
	}
	msec_t now = rbtTime_curr(r);
	if(now - stat->start_tm > stat->duration) {
		memcpy(out, stat, sizeof(struct StatRunTime));
		stat_rt_restart(r);
		return 1;
	}
	return 0;
}

void rbtStat_rt_add(struct rabbit * r, const TString * module, msec_t msec, int is_script, int is_ctx, int is_all)
{
	int flag = 0;
	if(is_script) {
		flag |= STAT_SCRIPT;
	}
	if(is_ctx) {
		flag |= STAT_CONTEXT;
	}
	if(is_all) {
		flag |= STAT_ALL_TIME;
	}
	rbtStat_rt_add_ex(r, NULL, module, msec, flag);
}

void rbtStat_rt_add_ex(struct rabbit * r, const TString * file, const TString * fun, int usec, int flag)
{
	if(!fun) {
		kLOG(r, 0, "[Warinning!] :%s，fun is NULL!\n", __FUNCTION__);
		return;
	}

	struct StatRunTime * stat = &r->stat_rt;

	if(!stat->count_tbl) {
		// 不进行统计
		return;
	}

	if(flag & STAT_SCRIPT) {
		stat->script_count++;
		stat->script_tm += usec;
	}

	const TString * name = fun;
	if(file) {
		static char buf[64];
		int pos = min(rbtS_len(file), 32);
		memcpy(buf, rbtS_gets(file), pos);
		buf[pos] = ':';
		memcpy(&buf[pos+1], rbtS_gets(fun), min(rbtS_len(fun) + 1, 62 - pos));
		buf[63] = 0;
		
		name = rbtS_new(r, buf);
	}

	Table * count_tbl = stat->count_tbl;
	Table * tm_tbl = stat->tm_tbl;
	if(flag & STAT_ALL_TIME) {
		count_tbl = stat->cl_count_all_tbl;
		tm_tbl = stat->cl_tm_all_tbl;
	}

	if(flag & STAT_CONTEXT) {
		count_tbl = stat->ctx_count_tbl;
		tm_tbl = stat->ctx_tm_tbl;
	}

	TValue * tv = rbtH_settstr(r, count_tbl, name);
	if(ttisnumber(tv)) {
		setnumvalue(tv, numbervalue(tv) + 1);
	} else {
		setnumvalue(tv, 1);
	}

	tv = rbtH_settstr(r, tm_tbl, name);
	if(ttisnumber(tv)) {
		setnumvalue(tv, numbervalue(tv) + usec);
	} else {
		setnumvalue(tv, usec);
	}
}

#endif // STAT_RUN_TIME
