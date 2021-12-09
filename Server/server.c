#include "server.h"

#include "rpc.h"
#include "player.h"

//#include "group.h"
#include "player.h"
#include "svr_connection.h"

static void traverse( GCObject * obj )
{
	struct GlobalState * gs = cast(struct GlobalState*, obj);

	if(gs->accounts) {
		rbtC_mark(cast(GCObject *, gs->accounts));
	}
	if(gs->players) {
		rbtC_mark(cast(GCObject *, gs->players));
	}

	if(gs->config) {
		rbtC_mark(cast(GCObject *, gs->config));
	}

	int i;
	for(i = 0; i < LOGICAL_SERVER_MAX_NUM; ++i) {
		struct LogicalServer * serv = &gs->servs[i];
		if(serv->id >= 0 && serv->c) {
			if(serv->groups) {
				rbtC_mark(serv->groups);
			}
		}
	}
	for(i = 0; i < ZONE_MAX_NUM; ++i) {
		struct Zone * zone = &gs->zones[i];
		if(zone->id > 0 && zone->name) {
			rbtC_mark(zone->name);
		}
	}
}

static void release( GCObject * obj )
{
	struct GlobalState * gs = cast(struct GlobalState*, obj);

	RFREE(gs->r,gs);
}

static struct GlobalState * gl_init( rabbit * r ) 
{
	struct GlobalState * gs = RMALLOC(r,struct GlobalState,1);

	rbtC_link(r, cast(GCObject *, gs), TUSERDATA);

	gs->gc_traverse = traverse;
	gs->gc_release = release;

	gs->accounts = rbtH_init(r, 0, 0);

	// 玩家的信息都在 [gs->accounts] 中存在，这里只做弱引用，不进行(GC)处理
	gs->players = rbtH_init(r,0,0);
	rbtH_weak(gs->players);

	gs->config = NULL;
	gs->gate_ip = -1;
	gs->gate_port = -1;

	gs->tick = 0;

	int i;
	for(i = 0; i < LOGICAL_SERVER_MAX_NUM; ++i) {
		struct LogicalServer * serv = &gs->servs[i];
		serv->id = -1;
		serv->c = NULL;
		serv->groups = NULL;
	}

	for(i = 0; i < ZONE_MAX_NUM; ++i) {
		struct Zone * zone = &gs->zones[i];
		zone->id = -1;
		zone->name = NULL;
		zone->serv_id = -1;
		zone->nuser = 0;
	}

	gs->admin = NULL;
	gs->gate = NULL;

	rbtStat_init(r, &gs->stat);

	gs->nuser = 0;

	return gs;
}

rabbit * rbtF_init() {
	rabbit * r = rabbit_init();

	r->max_conns = 10000;

	r->_G = gl_init(r);

	r->rpc_process = rbtF_rpc_process;

	r->conn_broken = rbtF_conn_broken;

	return r;
}

struct LogicalServer * rbtF_get_serv(rabbit * r, int id)
{
	if(id < 0 || id >= LOGICAL_SERVER_MAX_NUM) {
		return NULL;
	}

	struct LogicalServer * serv = &G(r)->servs[id];
	if(serv->id != id || !serv->c) {
		return NULL;
	}

	return serv;
}

struct LogicalServer * rbtF_get_serv_by_zone(rabbit * r, int id)
{
	if(id < 0 || id >= ZONE_MAX_NUM) {
		return NULL;
	}

	struct Zone * zone = &G(r)->zones[id];

	if(zone->id != id) {
		return NULL;
	}

	return rbtF_get_serv(r, zone->serv_id);
}

struct Zone * rbtF_get_zone(rabbit * r, int zone_id)
{
	if(zone_id < 0 || zone_id >= ZONE_MAX_NUM) {
		return NULL;
	}

	struct Zone * zone = &G(r)->zones[zone_id];
	if(zone->id != zone_id) {
		return NULL;
	}

	return zone;
}

void rbtF_send_serv(rabbit * r, int id, Packet * pkt)
{
	struct LogicalServer * serv = rbtF_get_serv(r, id);
	if(!serv) {
		return;
	}
	serv->stat.npkt_sent++;
	serv->stat.sent_size += rbtP_size(pkt);

	rbtNet_send(r, serv->c, pkt);
}

void rbtF_send_serv_by_zone(rabbit * r, int zone_id, Packet * pkt)
{
	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, zone_id);
	if(serv) {
		rbtF_send_serv(r, serv->id, pkt);
	}
}

void rbtF_send_player(rabbit * r, Player * ply, Packet * pkt)
{
	ply->stat.npkt_sent++;
	ply->stat.sent_size += rbtP_size(pkt);

	G(r)->stat.npkt_sent++;
	G(r)->stat.sent_size += rbtP_size(pkt);

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, ply->account);
	if (G(r)->gate) {
		rbtNet_send(r, G(r)->gate, pkt);
	}
}

void rbtF_send_player_account(rabbit * r, int account, Packet * pkt)
{
	struct Player * ply = rbtF_player_get_account(r, account);
	if(!ply) {
		kLOG(r, 0, "[Error]给玩家发包, Account(%d), 不存在！\n", account);
		return;
	}

	rbtF_send_player(r, ply, pkt);
}

void rbtF_send_player_pid(rabbit * r, int pid, Packet * pkt)
{
	struct Player * ply = rbtF_player_get_pid(r, pid);
	if(!ply) {
		kLOG(r, pid, "[Error]给玩家发包, Pid(%d), 不存在！\n", pid);
		return;
	}

	rbtF_send_player(r, ply, pkt);
}

