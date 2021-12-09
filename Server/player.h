#ifndef player_h_
#define player_h_

#include "server.h"
//#include "group.h"

enum PlayerState {
	E_PlayerState_OK,
	E_PlayerState_Saving,
	E_PlayerState_Twice,
};


/*
 *	Player 代表一个登录用户
 *
 */
typedef struct Player {

	int account;	// 账号id
	int pid;	// 角色id

	struct timeval come_tm;

	int zone_id;	// 在哪个 Zone 里

	// 组信息 - 玩家同时只能在一个组里，单链表足以
	int curr_gid;
	struct Player * curr_group_next;

	// 所有用户(同一个逻辑服务器里的)都链在一起，用于发送全局消息
	struct list_head player_list;


	// 世界消息 - 有CD
	time_t world_msg_tm;

	// 发包统计
	struct Statistic stat;

	// 状态
	int state;

} Player;


/*
 *	用户登录成功后，新建一个 Player，这个 Player 一直存在，直到 Connectin 断开连接
 *
 *	@param r
 *	@param c
 *	@param account 
 */
Player * rbtF_player_init( rabbit * r, int account );


/*
 *	根据用户 ID  得到 这个用户 Player 或 NULL
 *
 *	@param r
 *	@param pid
 */
Player * rbtF_player_get_pid( rabbit * r, int pid );

Player * rbtF_player_get_account( rabbit * r, int account );


/*
 *	用户断线
 *
 *	@param r
 *	@param player
 *	@param force	-- 是用户断线，还是 ‘管理服务器’ 发过来 ‘用户在其他地方登陆’ 令其强制断线
 */
int rbtF_player_abort( rabbit * r, Player * player );
void rbtF_player_destroy(rabbit * r, Player * player);

void rbtF_player_leave_group(rabbit * r, Player * player);

void rbtF_player_enter_group(rabbit * r, Player * player, int gid);

#endif

