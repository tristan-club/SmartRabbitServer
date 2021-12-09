#ifndef rabbit_h_
#define rabbit_h_

#include "common.h"
#include "list.h"
#include "statistic.h"

typedef struct stringtable {
	GCObject ** table;
	size_t used;
	size_t size;
}stringtable;

struct GlobalState;

struct rabbit {

	/* string table */
	stringtable stbl;
	const TString * empty_str;

	/* gc */
//	GCObject * gclist;
//	GCObject * gray;
	struct list_head gclist;
	struct list_head gray;
	struct list_head green;
	Queue * gc_queue;

	int mem;
	size_t obj;
	int currentwhite;
	int gc_status;

	/* mem dump */
	int mem_dump;
	void(* mem_dump_fun)(rabbit * r, int other_mem);

	/* network */
	NetManager * net_mgr;
	int(* conn_broken)( rabbit *, Connection * c );

	/* configuation */
	Table * config;
	int auth;
	int max_conns;

	int little_endian;
	int console;

	/* mysql */
	void * mysql;

	Connection * data;
	Table * data_pool;

	/* redis */
	Connection * redis;

	/* remote call */
	RpcManager * rpc;
	int(* rpc_process)( rabbit * r, Connection * c, int fun_id, Packet * pkt);

	/* user data */
	struct GlobalState * _G;

	/* 启动时间 */
	struct timeval tm;

	/* as client */
	int is_client;

	/* script */
	int debug_is_script_end;

	/* 统计 脚本等运行时间 */
	struct StatRunTime stat_rt;
};

// 自从开机，运行了多少毫秒了
msec_t rbtTime_curr(rabbit * r);

#define G(r)	cast(struct GlobalState *, r->_G)
#define EmptyString(r) ((r)->empty_str)

#define gco2str(o) cast(TString *, o)
#define gco2tbl(o) cast(Table *, o)
#define gco2buffer(o) cast(Buffer *, o)
#define gco2queue(o) cast(Queue *, o)
#define gco2lru(o) cast(Lru *, o)

#define gco2conn(o) cast(Connection *, o)

#define gco2pkt(o) cast(Packet *, o)
#define gco2netmgr(o) cast(NetManager *, o)
#define gco2stream(o) cast(stream *, o)

#define gco2ud(o) cast(UserData *, o)

#define gco2cl(o) cast(Closure *, o)
#define gco2script(o) cast(Script *, o)
#define gco2proto(o) cast(Proto *, o)

#endif

