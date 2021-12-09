#include "rpc.h"

//#include "group.h"
#include "player.h"
#include "twice_login.h"

#include "svr_connection.h"

static int handle_buddy( rabbit * r, Connection * c, Packet * pkt, int raw )
{
	short int fun = 0;
	if(raw) {
		rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 6);
		if(rbtP_readShort(pkt, &fun) < 0) {
			kLOG(r, 0, "[Error] Server Call Buddy Raw Error. 没有 Fun\n");
			return 0;
		}
	} else {
		rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 4);
	}

	int account;
	if(rbtP_readInt(pkt, &account) < 0) {
		kLOG(r, 0, "[Error] Server Call Buddy 错误，没有account\n");
		return 0;
	}
	int is_account = (account & 0x80000000) != 0;
	account &= 0x7FFFFFFF;

	rbtP_set_fun(pkt, fun);

	if(raw) {
		rbtP_erase(pkt, 6);
	} else {
		rbtP_erase(pkt, 4);
	}

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, 0);	// 服务器给客户端发的数据包，末尾4个0

	rbtP_encode(pkt);

	if(is_account) {
		rbtF_send_player_account(r, account, pkt);
	} else {
		rbtF_send_player_pid(r, account, pkt);
	}

	return 0;
}

#define GROUP_CALL_FLAG_NORMAL	0
#define GROUP_CALL_FLAG_RAW	1
#define GROUP_CALL_FLAG_BUT	2

static int handle_group_call(rabbit * r, Connection * c, Packet * pkt, int flag)
{
	short int fun = 0;
	int but_pid = 0;
	if(flag & GROUP_CALL_FLAG_RAW) {
		if(flag & GROUP_CALL_FLAG_BUT) {
			rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 10);
			if(rbtP_readInt(pkt, &but_pid) < 0) {
				kLOG(r, 0, "[Error] Server Call Group Raw But 错误，没有but_pid\n");
				return 0;
			}
		} else {
			rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 6);
		}
		if(rbtP_readShort(pkt, &fun) < 0) {
			kLOG(r, 0, "[Error] Server Call Group Raw 错误，没有Fun\n");
			return 0;
		}
	} else {
		rbtP_seek(pkt, PKT_DATA_SIZE(pkt) - 4);
	}

	int gid;
	if(rbtP_readInt(pkt, &gid) < 0) {
		kLOG(r, 0, "[Error] Server Call Group 错误，没有gid\n");
		return 0;
	}

	rbtP_set_fun(pkt, fun);

	if(flag & GROUP_CALL_FLAG_RAW) {
		if(flag & GROUP_CALL_FLAG_BUT) {
			rbtP_erase(pkt, 10);
		} else {
			rbtP_erase(pkt, 6);
		}
	} else {
		rbtP_erase(pkt, 4);
	}

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, 0);	// 服务器发的数据包，末尾为0（4字节）

	struct ConnectionX * connX = rbtNet_get_x(c);
	struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);

	const TValue * tv = rbtH_getnum(r, serv->groups, gid);

	if(ttisnil(tv)) {
		return 0;
	}

	rbtP_encode(pkt);

	struct Player * ply = cast(Player *, gcvalue(tv));
	while(ply) {
		struct Player * curr = ply;
		ply = ply->curr_group_next;

		if(curr->curr_gid != gid) {
			kLOG(r, 0, "[Error]玩家(%d)在这个组里，但组ID不同(%x != %x)！\n", curr->pid, curr->curr_gid, gid);
			continue;
		}
		if(curr->pid == but_pid) {
			kLOG(r, 0, "[LOG]InvokeGroupBut玩家(%d)，组(%x) 忽略！\n", curr->pid, curr->curr_gid);
			continue;
		}

		rbtF_send_player(r, curr, pkt);
	}

	return 0;
}

static int handle_global_message(rabbit * r, Connection * c, Packet * pkt)
{
	struct ConnectionX * connX = rbtNet_get_x(c);
	struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);

	if(list_empty(&serv->player_list)) {
		// 无玩家，不处理
		kLOG(r, 0, "[LOG]收到一个系统消息！当前没有玩家，忽略！\n");
		return 0;
	}

	rbtP_grab(pkt);
	int i;
	struct GlobalMessage * gm = NULL;
	for(i = 0; i < array_length(serv->broadcast); ++i) {
		gm = array_at(serv->broadcast, i);
		if(!gm->pkt) {
			break;
		}
	}

	if(!gm) {
		gm = array_push(serv->broadcast);
	}
	gm->pkt = pkt;
	gm->next = serv->player_list.next;

	serv->nbroadcast++;

	rbtP_seek_end(pkt);
	rbtP_writeInt(pkt, 0);

	rbtP_encode(pkt);

	kLOG(r, 0, "[LOG]收到一个系统消息！\n");

	return 0;
}

static int handle_add_player_to_group(rabbit * r, Connection * c, Packet * pkt)
{
	int pid, gid;
	if(rbtP_readInt(pkt, &pid) < 0) {
		kLOG(r, 0, "[Error] Server 将玩家加入组，缺少 PID\n");
		return 0;
	}
	if(rbtP_readInt(pkt, &gid) < 0) {
		kLOG(r, pid, "[Error]将玩家(%d)加入组，缺少 GID\n", pid);
		return 0;
	}

	kLOG(r, pid, "[LOG]将玩家(%d)加入组(%x)\n", pid, gid);

	struct Player * ply = rbtF_player_get_pid(r, pid);
	if(!ply) {
		kLOG(r, pid, "[Error]将玩家(%d)加入组(%x)，玩家不存在！\n", pid, gid);
		return 0;
	}

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	if(!serv || serv->c != c) {
		kLOG(r, pid, "[Error]玩家不在这个服务器上！\n");
		return 0;
	}

	rbtF_player_enter_group(r, ply, gid);

	return 0;
}

static int handle_rm_player_from_group(rabbit * r, Connection * c, Packet * pkt)
{
	int pid, gid;
	if(rbtP_readInt(pkt, &pid) < 0) {
		kLOG(r, 0, "[Error]将玩家移出组，缺少PID\n");
		return 0;
	}
	if(rbtP_readInt(pkt, &gid) < 0) {
		kLOG(r, pid, "[Error]将玩家(%d)移出组，缺少GID\n", pid);
		return 0;
	}

	kLOG(r, pid, "[LOG]服务器将玩家(%d)移出组(%x)\n", pid, gid);

	struct Player * ply = rbtF_player_get_pid(r, pid);
	if(!ply) {
		kLOG(r, pid, "[Error]玩家不存在！\n");
		return 0;
	}

	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, ply->zone_id);
	if(!serv || serv->c != c) {
		kLOG(r, pid, "[Error]玩家不在这个服务器上！\n");
		return 0;
	}

	if(ply->curr_gid != gid) {
		kLOG(r, pid, "玩家现在不在这个组(%x)里，当前组：%x！\n", gid, ply->curr_gid);
		return 0;
	}

	rbtF_player_leave_group(r, ply);

	return 0;
}

static int handle_create_zone(rabbit * r, Connection * c, Packet * pkt)
{
	int zone_id;
	TString * zone_name;
	if(rbtP_readInt(pkt, &zone_id) < 0) {
		kLOG(r, 0, "[Error] 创建Zone，缺少 ZoneID\n");
		return 0;
	}
	if(rbtP_readString(pkt, &zone_name) < 0) {
		kLOG(r, 0, "[Error] 创建Zone，缺少 ZoneName\n");
		return 0;
	}
	struct ConnectionX * connX = rbtNet_get_x(c);

	kLOG(r, 0, "[LOG]逻辑服务器(%d)创建Zone(%d : %s)\n", connX->a2, zone_id, rbtS_gets(zone_name));

	struct Zone * zone = rbtF_get_zone(r, zone_id);
	if(zone) {
		kLOG(r, 0, "[Error]这个Zone已经存在了(%s),在服务器(%d) 上！\n", rbtS_gets(zone->name), zone->serv_id);
		if(zone->serv_id == connX->a2) {
			kLOG(r, 0, "[LOG]服务器断开重连，还是原来的Zone(%d)，重新将用户登录到逻辑服务器上！\n", zone_id);
			struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);
			int id = -1;
			TValue key, val;
			Table * argv = rbtH_init(r, 1, 1);
			while(1) {
				id = rbtH_next(r, G(r)->players, id, &key, &val);
				if(id < 0) {
					break;
				}
				struct Player * ply = cast(struct Player *, gcvalue(&val));
				if(ply->zone_id == zone_id) {
					// 先进Zone
					Packet * pkt_ = rbtP_init(r);
					rbtP_set_fun(pkt_, SERVER_ENTER_ZONE | PKT_FUN_MASK);
					rbtP_writeInt(pkt_, ply->account);
					rbtP_writeInt(pkt_, zone_id);

					rbtNet_send(r, c, pkt_);
					rbtP_drop(pkt_);

					// 然后选择角色
					pkt_ = rbtP_init(r);
					rbtP_set_fun(pkt_, (SERVER_CALL << 8 & 0x1F00) | PKT_FUN_MASK);
					rbtP_writeString(pkt_, "system.SelectRole");
					setnumvalue(rbtH_setstr(r, argv, "pid"), ply->pid);
					rbtP_writeTable(pkt_, argv);
					rbtP_writeInt(pkt_, ply->account);
					rbtP_writeInt(pkt_, ply->pid);

					rbtNet_send(r, c, pkt_);
					rbtP_drop(pkt_);

					zone->nuser++;
					serv->nuser++;
				}
			}
		}
		return 0;
	}

	if(zone_id < 1 || zone_id >= ZONE_MAX_NUM) {
		kLOG(r, 0, "[Error]ZoneID(%d)超出界限(%d)！\n", zone_id, ZONE_MAX_NUM);
		return 0;
	}

	zone = &G(r)->zones[zone_id];

	zone->id = zone_id;
	zone->name = zone_name;
	zone->serv_id = connX->a2;
	zone->nuser = 0;

	return 0;
}

static int handle_buddy_bind(rabbit * r, Connection * c, Packet * pkt)
{
	int account, pid;
	if(rbtP_readInt(pkt, &account) < 0) {
		return 0;
	}
	if(rbtP_readInt(pkt, &pid) < 0) {
		return 0;
	}

	struct Player * ply = rbtF_player_get_account(r, account);
	if(!ply || ply->state != E_PlayerState_OK) {
		kLOG(r, pid, "[Error] 账号(%d) 和玩家(%d) 绑定，玩家不存在(%p)，或者玩家的state不是OK！", account, pid, ply);
		if(ply) {
			kLOG(r, pid, "---- state:%d\n", ply->state);
		}
		return 0;
	}
	
	struct ConnectionX * connX = rbtNet_get_x(c);

	ply->pid = pid;

	setpvalue(rbtH_setnum(r, G(r)->players, pid), ply);

	kLOG(r, pid, "账号(%d)和玩家(%d)绑定！\n", account, pid);

	if(!list_empty(&ply->player_list)) {
		// 用户原来链好的
		kLOG(r, pid, "[Error]%s:玩家(%d:%d)之前链好的！\n", __FUNCTION__, account, pid);
		return 0;
	}

	struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);
	list_insert(&serv->player_list, &ply->player_list);

	return 0;
}

static int handle_server_buddy_abort_ok(rabbit * r, Connection * c, Packet * pkt)
{
	int account;
	if(rbtP_readInt(pkt, &account) < 0) {
		kLOG(r, 0, "[Warning]%s:account 读取失败！\n", __FUNCTION__);
		return 0;
	}

	rbtF_player_save_ok(r, account);
	
	return 0;
}

static int handle_server_stat(rabbit * r, Connection * c, Packet * pkt)
{
	struct ConnectionX * connX = rbtNet_get_x(c);
	struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);

	int mem;
	if(rbtP_readInt(pkt, &mem) < 0) {
		return 0;
	}

	serv->stat.mem = mem;
	return 0;
}

int rbtF_rpc_server( rabbit * r, Connection * c, int fun, Packet * pkt )
{
	switch(fun) {
		case CONN_FOR_SERVER_BUDDY_CALL:
			handle_buddy(r, c, pkt, 0);
			return 0;

		case CONN_FOR_SERVER_BUDDY_CALL_RAW:
			handle_buddy(r, c, pkt, 1);
			return 0;

		case CONN_FOR_SERVER_GROUP_CALL:
			handle_group_call(r, c, pkt, GROUP_CALL_FLAG_NORMAL);
			return 0;

		case CONN_FOR_SERVER_GROUP_CALL_RAW:
			handle_group_call(r, c, pkt, GROUP_CALL_FLAG_RAW);
			return 0;

		case CONN_FOR_SERVER_GROUP_CALL_RAW_BUT:
			handle_group_call(r, c, pkt, GROUP_CALL_FLAG_RAW | GROUP_CALL_FLAG_BUT);
			return 0;

		case CONN_FOR_SERVER_GLOBAL_MESSAGE:
			handle_global_message(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_ADD_PLAYER_TO_GROUP:
			handle_add_player_to_group(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_REMOVE_PLAYER_FROM_GROUP:
			handle_rm_player_from_group(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_CREATE_ZONE:
			handle_create_zone(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_BUDDY_BIND:
			handle_buddy_bind(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_STATISTIC:
			handle_server_stat(r, c, pkt);
			return 0;

		case CONN_FOR_SERVER_BUDDY_ABORT_OK:
			handle_server_buddy_abort_ok(r, c, pkt);
			return 0;

		default:
			kLOG(r, 0, "[Error]Server Call(%d) Unknown\n", fun);
			return 0;
	}
	
	return 0;
}

