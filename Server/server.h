#ifndef server_h_
#define server_h_

/*
   ‘连接服务器’是直接和‘客户端’相连的，可以称之为‘Star War Battlefront’

   用 star war battleFront 中的 'F' 作为 前缀

*/

#include "../rbtlib/inclib.h"


/*
 *	‘连接服务器’ 初始化
 *
 */
rabbit * rbtF_init();



/*
 *	‘连接服务器’ 的全局性运行环境
 *
 *	一个‘连接服务器’最多可以连接 30 个‘逻辑服务器’，已经足够了吧
 *
 *	每个‘连接服务器’都要连接‘家园服务器’ 和 ‘管理服务器’
 */
#define LOGICAL_SERVER_MAX_NUM	30

#define ZONE_MAX_NUM	30	// 每个连接服务器上，最多有多少个Zone

struct Player;

struct LogicalServer {
	int id;

	int nuser;	// 玩家数

	Connection * c;	// 连接

	/* Time */
	struct timeval come_tm;	// 连接时间

	/* Statistic */
	struct Statistic stat;

	/* Group */
	Table * groups;	// 这个服务上的组 gid --> first player

	/* 这个服务器上的用户，链在一起 */
//	struct Player * first_player;
//	struct Player * last_player;

	struct list_head player_list;

	/* 在这个服务器上，全局广播的数据 */
	struct Array * broadcast;
	int nbroadcast;
};

struct Zone {
	int id;

	TString * name;	// Zone 名称

	int serv_id;	// 在哪个逻辑服务器上

	int nuser;	// 玩家数
};

struct GlobalMessage {
	Packet * pkt;		// 需要广播的消息 
	struct list_head * next;
};

struct GlobalState {
	CommonHeader;

	Table * config;

	int gate_ip;
	int gate_port;

	Table * players;
	Table * accounts;
	int nuser;

	int key;
	int id;

	int tick;
	struct timeval globalTime;	// 全局时间

	struct LogicalServer servs[LOGICAL_SERVER_MAX_NUM];
	struct Zone zones[ZONE_MAX_NUM];

	// Admin
	Connection *admin;
	Connection *gate;

	// 统计客户端发来、发出的数据
	struct Statistic stat;
};

struct LogicalServer *
rbtF_get_serv(rabbit * r, int id);

void
rbtF_send_serv(rabbit * r, int id, Packet * pkt);

struct LogicalServer *
rbtF_get_serv_by_zone(rabbit * r, int zone_id);

struct Zone *
rbtF_get_zone(rabbit * r, int zone_id);

void
rbtF_send_player(rabbit * r, struct Player * ply, Packet * pkt);

void
rbtF_send_player_account(rabbit * r, int account, Packet * pkt);

void
rbtF_send_player_pid(rabbit * r, int pid, Packet * pkt);

#endif

