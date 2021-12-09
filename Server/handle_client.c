#include "rpc.h"

#include "svr_connection.h"
#include "player.h"

static void handle_call_server( rabbit * r, struct Player * ply, Packet * pkt, int scene, int t, int sub )
{
	if(scene) {
		rbtP_set_fun(pkt, ((SERVER_CALL_SCENE << 8) | sub) | t | PKT_FUN_MASK);
	} else {
		rbtP_set_fun(pkt, ((SERVER_CALL << 8) | sub) | t | PKT_FUN_MASK);
	}
	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, ply->account);
	rbtP_writeInt(pkt, ply->pid);

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	rbtF_send_serv(r, serv->id, pkt);
}

static void handle_call_other( rabbit * r, struct Player * ply, Packet * pkt, int sub)
{
	rbtP_set_fun(pkt, ((SERVER_CALL_OTHER << 8) | sub) | PKT_FUN_MASK | 0x2000);

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, ply->account);
	rbtP_writeInt(pkt, ply->pid);

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	rbtF_send_serv(r, serv->id, pkt);
}

static void handle_call_group( rabbit * r, struct Player * ply, Packet * pkt, int pid )
{
	if(ply->pid <= 0) {
		// 如果没有进行账号&角色绑定，不能广播消息
		kLOG(r, 0, "[Error] %s : ply->pid(%d) <= 0!\n", __FUNCTION__, ply->pid);
		return;
	}

	if(pid <= 0) {
		//若不是机器人进行的调用，则不能进行广播
		if(ply->account < 2000 || ply->account >= 3000) {
			kLOG(r, 0, "[Error] %s : 玩家仿照服务器广播，但ply->account(%d) 不是机器人!\n", __FUNCTION__, ply->account);
			return;
		}
	}

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, pid);

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	const TValue * tv = rbtH_getnum(r, serv->groups, ply->curr_gid);
	if(ttisnil(tv)) {
		kLOG(r, ply->pid, "[Error]玩家(%d)广播，不在组中？(%x)\n", ply->pid, ply->curr_gid);
		return;
	}

	rbtP_encode(pkt);

	struct Player * other = cast(Player *, pvalue(tv));
	while(other) {
		if(other != ply) {
			// 不广播给自己
			rbtF_send_player(r, other, pkt);
		}
		other = other->curr_group_next;
	}
}

static void handle_call_buddy( rabbit * r, struct Player * ply, Packet * pkt )
{
	if(ply->pid <= 0) {
		//如果没有进行 账号&角色 绑定，不能发消息
		return;
	}
	rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 4);
	int bid;
	if(rbtP_readInt(pkt, &bid) < 0) {
		kLOG(r, ply->pid, "[Error]Client call buddy Error! 没有Bid\n");
		return;
	}
	struct Player * other = rbtF_player_get_pid(r, bid);

	rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 4);
	rbtP_writeInt(pkt, ply->pid);

	if(!other) {
		kLOG(r, ply->pid, "[LOG] Client(%d) call buddy 玩家(%d)不在线！\n", ply->pid, bid);
		return;
	}

	rbtP_encode(pkt);

	rbtF_send_player(r, other, pkt);
	return;
}

static void handle_call_world_msg( rabbit * r, struct Player * ply, Packet * pkt )
{
	if(ply->pid <= 0) {
		return;
	}

	time_t now = time(NULL);
	if(now - ply->world_msg_tm < 10) {
		kLOG(r, ply->pid, "[LOG]客户端发送世界消息，在10秒限制中！(pid:%d, zone_id:%d, last:%zu, now:%zu)\n", ply->pid, ply->zone_id, ply->world_msg_tm, now);
		return;
	}

	ply->world_msg_tm = now;

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, ply->pid);

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	if(!serv) {
		kLOG(r, ply->pid, "[Error]客户端发送世界消息，不在逻辑服务器里？(pid:%d, zone_id:%d)\n", ply->pid, ply->zone_id);
		return;
	}

	rbtP_encode(pkt);

	struct list_head * p;
	list_foreach(p, &serv->player_list) {
		struct Player * other = list_entry(p, struct Player, player_list);
		if(other != ply) {
			// 不广播给自己
			rbtF_send_player(r, other, pkt);
		}
	}
}

static void handle_keep_alive( rabbit * r, Connection * c, Packet * pkt )
{
}

// 处理从 Gate 发来的数据包
int rbtF_rpc_client( rabbit * r, Connection * c, int fun, Packet * pkt )
{
	struct ConnectionX * conn = rbtNet_get_x(c);
	int acc = -1;
	rbtP_seek(pkt, PKT_DATA_SIZE(pkt)-4);
	if (rbtP_readInt(pkt, &acc) < 0) {
		kLOG(r, 0, "[Error] 没有玩家 Account 信息?\n");
		return 0;
	}
	rbtP_erase(pkt, sizeof(int));

	struct Player * ply = rbtF_player_get_account(r, acc);
	if(!ply) {
		kLOG(r, 0, "[Error]用户(Account:%d)发信息，没有这个用户？？？\n", conn->a2);
		return 0;
	}

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	if(!serv) {
		kLOG(r, ply->pid, "[Error]玩家发信息，不在Zone里啊？？？\n");
		return 0;
	}

	rbtP_decode(pkt);

	int t = fun & 0x2000;
	int sub = fun & 0xFF;
	fun = (fun >> 8) & 0x1F;

	switch(fun){

		case CLIENT_KEEP_ALIVE:
			handle_keep_alive(r, c, pkt);
			break;

		case CLIENT_CALL_SERVER_SYSTEM:
			handle_call_server(r, ply, pkt, 0, t, sub);
			break;

		case CLIENT_CALL_SERVER_SCENE:
			handle_call_server(r, ply, pkt, 1, t, sub);
			break;

		case CLIENT_CALL_SERVER_OTHER:
			handle_call_other(r, ply, pkt, sub);
			break;

		case CLIENT_CALL_GROUP:
			handle_call_group(r, ply, pkt, ply->pid);
			break;

		case CLIENT_ROBOT_CALL_GROUP:
			handle_call_group(r, ply, pkt, 0);
			break;

		case CLIENT_CALL_WORLD_MSG:
			handle_call_world_msg(r, ply, pkt);
			break;

		case CLIENT_CALL_BUDDY:
			handle_call_buddy(r, ply, pkt);
			break;

		default:
			kLOG(r, ply->pid, "[Error]Client Rpc. Unknow Fun(%d)\n", fun);
			break;
	}

	return 0;
}

