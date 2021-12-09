#include "player.h"
#include "svr_connection.h"
//#include "group.h"

Player * rbtF_player_get_account( rabbit * r, int account )
{
	const TValue * tv = rbtH_getnum(r, G(r)->accounts, account);
	if(ttisnil(tv)) {
		return NULL;
	}

	return cast(Player *, pvalue(tv));
}

Player * rbtF_player_get_pid( rabbit * r, int pid )
{
	const TValue * tv = rbtH_getnum(r, G(r)->players, pid);
	if(ttisnil(tv)) {
		return NULL;
	}

	return cast(Player *, pvalue(tv));
}

Player * rbtF_player_init( rabbit * r, int account )
{
	struct Player * ply = RMALLOC(r, struct Player, 1);

	ply->account = account;
	ply->pid = -1;

	ply->curr_gid = -1;
	ply->curr_group_next = NULL;

	gettimeofday(&ply->come_tm, NULL);

	setpvalue(rbtH_setnum(r, G(r)->accounts, account), ply);

	rbtStat_init(r, &ply->stat);

	list_init(&ply->player_list);

	ply->world_msg_tm = -1000;

	return ply;
}

void rbtF_player_destroy(rabbit * r, Player * player) 
{
	rbtH_rmnum(r, G(r)->accounts, player->account);
	RFREE(r, player);
}

void rbtF_player_enter_group(rabbit * r, Player * player, int gid)
{
	if(player->curr_gid > 0) {
		rbtF_player_leave_group(r, player);
	}

	player->curr_gid = gid;

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, player->zone_id);
	if(!serv) {
		kLOG(r, player->pid, "玩家(%d)进组(%x)，没有服务器？\n", player->pid, player->zone_id);
		return;
	}

	TValue * tv = cast(TValue *, rbtH_getnum(r, serv->groups, gid));
	if(!ttisp(tv)) {
		player->curr_group_next = NULL;
		setpvalue(rbtH_setnum(r, serv->groups, gid), player);
		return;
	}

	player->curr_group_next = cast(Player *, pvalue(tv));
	setpvalue(tv, cast(GCObject *, player));
}

void rbtF_player_leave_group(rabbit * r, Player * player)
{
	struct Player * other;

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, player->zone_id);
	if(!serv) {
		return;
	}

	const TValue * tv = rbtH_getnum(r, serv->groups, player->curr_gid);
	if(ttisnil(tv)) {
		return;
	}

	// 广播，用户退组
	other = cast(Player *, pvalue(tv));
	struct Player * prev = NULL;

	Packet * pkt = rbtP_init(r);
	rbtP_set_fun(pkt, RAW_PLAYER_LEAVE_SCENE);
	rbtP_writeIntAMF3(pkt, player->pid);
	rbtP_writeInt(pkt, 0);			// 服务器发送的数据包

	rbtP_encode(pkt);

	int has_prev = 0;

	while(other) {
		if(other == player) {
			if(prev) {
				prev->curr_group_next = player->curr_group_next;
				has_prev = 1;
			}
		} else {
			rbtF_send_player_account(r, other->account, pkt);
		}

		prev = other;
		other = other->curr_group_next;
	}

	rbtP_drop(pkt);


	// 如果当前用户是第一个，更改链表头指针
	if(!has_prev) {
		if(cast(Player *, pvalue(tv)) != player) {
			kLOG(r, player->pid, "[Error]用户退组出错！用户不在这个组里？Zone(%d), 组(%x)\n", player->zone_id, player->curr_gid);
		} else {
			if(player->curr_group_next) {
				setpvalue(cast(TValue *, tv), player->curr_group_next);
			} else {
				rbtH_rmnum(r, serv->groups, player->curr_gid);
			}
		}
	}

	player->curr_gid = -1;
	player->curr_group_next = NULL;
}

/* 	
 *	清除一个 Player :
 *
 *	1. 从组里退出
 *
 *	2. 从全局链表中删去
 *
 *	3. 调整全局消息 
 *
 *	4. **不**释放内存
 *
 */
int rbtF_player_abort( rabbit * r, Player * player )
{
	if(!player) {
		return 0;
	}

	int i;

	if(player->pid > 0) {
		G(r)->nuser--;
		rbtH_rmnum(r, G(r)->players, player->pid);
	}

	// 调整全局消息，下一个接受者，如果是player，则顺延
	struct Zone * zone = rbtF_get_zone(r, player->zone_id);
	if(zone) {
		struct LogicalServer * serv = rbtF_get_serv(r, zone->serv_id);
		if(serv) {
			for(i = 0; i < array_length(serv->broadcast); ++i) {
				struct GlobalMessage * gm = array_at(serv->broadcast, i);
				if(gm->next == &player->player_list) {
					gm->next = player->player_list.next;
				}
			}
		} else {
			kLOG(r, player->pid, "[Warning]玩家退出，没有这个server(%d), zone:%d！\n", zone->serv_id, player->zone_id);
		}
	} else {
		kLOG(r, player->pid, "[Warning]玩家退出，没有这个zone(%d)！\n", player->zone_id);
	}

	// 从服务器全局链表中删除
	list_del(&player->player_list);

	// 玩家从当前组退出
	rbtF_player_leave_group(r, player);

	return 0;
}

