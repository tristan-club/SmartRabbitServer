#include "rpc.h"

//#include "group.h"
#include "player.h"

#include "svr_connection.h"

static void admin_statistic(rabbit * r, Connection * c, Packet * tmp)
{
	int time;

	Packet * pkt = rbtP_init(r);

	// 统计服务器
	int pos = rbtP_curr_offset(pkt);

	int nserv = 0;

	rbtP_writeInt(pkt, 0);

	int i;
	for(i = 0; i < LOGICAL_SERVER_MAX_NUM; ++i) {
		struct LogicalServer * serv = &G(r)->servs[i];
		if(serv->id <= 0 || !serv->c) {
			continue;
		}

		time = G(r)->globalTime.tv_sec - serv->come_tm.tv_sec;

		rbtP_writeInt(pkt, serv->id);
		rbtP_writeInt(pkt, serv->nuser);
		rbtP_writeInt(pkt, time);
		rbtP_writeInt(pkt, serv->stat.npkt_recv);
		rbtP_writeInt(pkt, serv->stat.npkt_sent);
		rbtP_writeInt(pkt, serv->stat.recv_size);
		rbtP_writeInt(pkt, serv->stat.sent_size);
		rbtP_writeInt(pkt, serv->stat.mem);

		nserv++;
	}

	int now = rbtP_curr_offset(pkt);

	rbtP_seek(pkt, pos);
	rbtP_writeInt(pkt, nserv);

	rbtP_seek(pkt, now);

	// 统计对外输入、输出
	rbtP_writeInt(pkt, G(r)->stat.npkt_recv);
	rbtP_writeInt(pkt, G(r)->stat.npkt_sent);
	rbtP_writeInt(pkt, G(r)->stat.recv_size);
	rbtP_writeInt(pkt, G(r)->stat.sent_size);
	rbtP_writeInt(pkt, rbtTime_curr(r));
	rbtP_writeInt(pkt, r->mem);

	rbtP_set_fun(pkt, 1 | PKT_FUN_MASK);
	rbtNet_send(r, c, pkt);
}

int rbtF_rpc_admin( rabbit * r, Connection * c, int fun, Packet * pkt )
{
	switch(fun) {
		case CONN_FOR_ADMIN_STATISTIC:
			admin_statistic(r, c, pkt);
			break;
				
		default:
			kLOG(r, 0, "[Error]Admin Call(%d) Unknown\n", fun);
			return 0;
	}
	
	return 0;
}

