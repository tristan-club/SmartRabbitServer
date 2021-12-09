#include "remote_call.h"
#include "connection.h"
#include "connection_struct.h"
#include "rabbit.h"
#include "string.h"
#include "packet.h"
#include "table.h"
#include "mem.h"
#include "pool.h"
#include "gc.h"

#define NR_RPC_INIT_PARAM	256

#define RPC_REQ_ID_MIN	1

static int g_nparam_free = 0;
static int g_nparam_total = 0;


struct rpc_param {
	unsigned int req_id;
	void(*f)( rabbit *, struct RpcParam *, Packet * );

	struct RpcParam param;

	struct list_head rpc_list;
	struct list_head conn_list;
};

int rbtRpc_debug_param_count()
{
	return g_nparam_total - NR_RPC_INIT_PARAM;
}

int rbtRpc_debug_param_mem()
{
	return rbtRpc_debug_param_count() * sizeof(struct rpc_param);
}

struct RpcManager {
	rabbit * r;

	unsigned int last_req_id;

	struct list_head free_param;
	struct list_head busy_param;

	struct rpc_param init_param[NR_RPC_INIT_PARAM];

	Table * hash;

}g_RpcManager;

static void init_param( struct rpc_param * param ) 
{
	param->req_id = RPC_REQ_ID_NO;
	param->f = NULL;
	
	memset(&param->param, 0, sizeof(struct RpcParam));
}

int rbtRpc_init( rabbit * r )
{
	r->rpc = &g_RpcManager;

	r->rpc->r = r;

	r->rpc->last_req_id = RPC_REQ_ID_MIN;

	list_init(&r->rpc->free_param);
	list_init(&r->rpc->busy_param);

	r->rpc->hash = rbtH_init(r, 32, 32);
	rbtH_weak(r->rpc->hash);

	int i;
	for(i = 0; i < NR_RPC_INIT_PARAM; ++i) {
		struct rpc_param * p = &r->rpc->init_param[i];

		list_init(&p->rpc_list);
		list_insert(&r->rpc->free_param, &p->rpc_list);
	}

	g_nparam_free = g_nparam_total = NR_RPC_INIT_PARAM;

	return 0;
}

static struct rpc_param * get_next_param( struct RpcManager * rpc, Connection * c )
{
	rabbit * r = rpc->r;

	struct rpc_param * param;
	struct list_head * head;

	if(list_empty(&rpc->free_param)) {
		param = RMALLOC(r, struct rpc_param, 1);
		g_nparam_total++;
	} else {
		head = list_first_entry(&rpc->free_param);
		list_del(head);
		param = list_entry(head, struct rpc_param, rpc_list);
		g_nparam_free--;
	}

	init_param(param);

	list_init(&param->rpc_list);
	list_init(&param->conn_list);

	list_insert(&rpc->busy_param, &param->rpc_list);
	list_insert(&c->rpc_param, &param->conn_list);

	param->req_id = rpc->last_req_id++;
	if(param->req_id >= PKT_REQ_MAX) {
		param->req_id = RPC_REQ_ID_MIN;
		rpc->last_req_id = param->req_id++;
	}

	setpvalue(rbtH_setnum(r, rpc->hash, param->req_id), param);

	return param;
}

static void free_param(struct RpcManager * mgr, struct rpc_param * param)
{
	rbtH_rmnum(mgr->r, mgr->hash, param->req_id);
	list_del(&param->conn_list);
	list_del(&param->rpc_list);
	list_insert(&mgr->free_param, &param->rpc_list);

	g_nparam_free++;
}

struct RpcParam * rbtRpc_call( rabbit * r, void * f, Connection * c, int fun_id, const char * fmt, ... )
{
	Packet * pkt = rbtP_init(r);

	if(fmt) {
		va_list va;
		va_start(va, fmt);
		const char * str;
		int i;
		double d;
		Table * tbl;
		const TString * ts;

		while(*fmt) {
			switch( *fmt ) {
				case 'd':
					i = va_arg(va, int);
					rbtP_writeInt(pkt, i);
					break;

				case 's':
					str = va_arg(va, const char *);
					rbtP_writeString(pkt, str);
					break;

				case 'S':
					ts = va_arg(va, const TString *);
					rbtP_writeStringLen(pkt, rbtS_gets(ts), rbtS_len(ts));
					break;

				case 'f':
					d = va_arg(va, double);
					rbtP_writeDouble(pkt, d);
					break;

				case 'h':
				case 'H':
					tbl = va_arg(va, Table *);
					rbtP_writeTable(pkt, tbl);
					break;

				default:
					kLOG(r, 0, "RPC. Invalid param : %c\n", *fmt);
					rbtP_drop(pkt);
					return 0;

			}

			fmt++;
		}

		va_end(va);
	}

	struct RpcParam * param = rbtRpc_packet(r, f, c, fun_id, pkt);
	rbtP_drop(pkt);
	
	return param;
}

struct RpcParam * rbtRpc_packet( rabbit * r, void * f, Connection * c, int fun, Packet * pkt )
{
	if( !c ) {
		kLOG(r, 0, "[Error]rpc 发包，但是connection已经关闭了,可能是收到了signal（10）");
		return;
	}

	if(fun < 1 || fun > PKT_FUN_MAX) {
		kLOG(r, 0, "[Error]错误！RPC fun 不在有效范围内！fun(%d)\n", fun);
		return NULL;
	}

	rbtP_grab(pkt);

	int req_id = RPC_REQ_ID_NO;

	struct RpcParam * param = NULL;

	if(f) {
		struct rpc_param * rpcparam = get_next_param( r->rpc, c );

		rpcparam->f = f;

		req_id = rpcparam->req_id;

		param = &rpcparam->param;
	}

	if(req_id == RPC_REQ_ID_NO) {
		rbtP_set_fun(pkt, fun | PKT_FUN_MASK);
	} else {
		rbtP_set_fun(pkt, fun | PKT_FUN_MASK | PKT_REQ_MASK);
		rbtP_seek_end(pkt);
		rbtP_writeShort(pkt, req_id);
	}

	rbtNet_send(r, c, pkt);

	rbtP_drop(pkt);

	return param;
}

int rbtRpc_apply( rabbit * r, Connection * c, size_t req_id, Packet * pkt )
{
	RpcManager * rpc = r->rpc;
	
	struct rpc_param * param;

	const TValue * tv = rbtH_getnum(r, rpc->hash, req_id);
	if(!ttisp(tv)) {
		kLOG(r, 0, "[Error]Rpc Back. Req ID Is Missing(%zu)\n", req_id);
		return 0;
	}

	param = cast(struct rpc_param *, pvalue(tv));

	void(*f)(rabbit *, struct RpcParam *, Packet *) = param->f;

	struct RpcParam * loc_param = &param->param;
	if(f) {
		f(r, loc_param, pkt);
	}

	free_param(rpc, param);

	return 0;
}

int rbtRpc_ret( rabbit * r, Connection * c, int req_id, const char * fmt, ... )
{
	if(req_id == RPC_REQ_ID_NO) {
		kLOG(r, 0, "[Error]警告！RPC 回调，无效的req_id(%d)\n", req_id);
		return 0;
	}

	if(req_id < RPC_REQ_ID_MIN || req_id > PKT_REQ_MAX) {
		kLOG(r, 0, "[Error]警告！RPC 回调，无效的req_id(%d)\n", req_id);
		return 0;
	}

	Packet * pkt = rbtP_init(r);
	rbtP_set_fun(pkt, req_id);

	if(fmt) {
		va_list va;
		va_start(va, fmt);
		const char * str;
		int i;
		double d;
		Table * tbl;
		const TString * ts;

		while(*fmt) {
			switch( *fmt ) {
				case 'd':
					i = va_arg(va, int);
					rbtP_writeInt(pkt, i);
					break;

				case 's':
					str = va_arg(va, const char *);
					rbtP_writeString(pkt, str);
					break;

				case 'S':
					ts = va_arg(va, const TString *);
					rbtP_writeString(pkt, rbtS_gets(ts));
					break;

				case 'f':
					d = va_arg(va, double);
					rbtP_writeDouble(pkt, d);
					break;

				case 'h':
				case 'H':
					tbl = va_arg(va, Table *);
					rbtP_writeTable(pkt, tbl);
					break;

				default:
					kLOG(r, 0, "[Error]RPC. Invalid param : %c\n", *fmt);
					rbtP_drop(pkt);
					return 0;

			}

			fmt++;
		}
	}

	rbtNet_send(r, c, pkt);

	rbtP_drop(pkt);

	return 0;
}

void rbtRpc_conn_broken(rabbit * r, Connection * c)
{
	struct RpcManager * mgr = r->rpc;
	struct list_head *head, *tmp;
	struct rpc_param * param;

	list_foreach_safe(head, tmp, &c->rpc_param) {
		param = list_entry(head, struct rpc_param, conn_list);
		free_param(mgr, param);
	}
}

void rbtRpc_traverse(rabbit * r, struct RpcManager * mgr)
{
	rbtC_mark(mgr->hash);

	struct list_head * head;
	struct rpc_param * param;

	list_foreach(head, &mgr->busy_param) {
		param = list_entry(head, struct rpc_param, rpc_list);
		if(param->param.obj) {
			rbtC_mark(param->param.obj);
		}
		if(param->param.obj2) {
			rbtC_mark(param->param.obj2);
		}
	}

	if(g_nparam_free < g_nparam_total) {
//		fprintf(stderr, "[测试]，当前有(%d)个RpcParam，空闲（%d)个，忙(%d)个!\n", g_nparam_total, g_nparam_free, g_nparam_total - g_nparam_free);
	}
}

