#include "rpc.h"
#include "svr_connection.h"
#include "player.h"
#include "twice_login.h"
#include "../rbtlib/open_login.h"

static int serv_reg(rabbit * r, Connection * c, Packet * pkt)
{
	rbtP_seek(pkt, 0);
	int key, id;
	if(rbtP_readInt(pkt, &key) < 0) {
		kLOG(r, 0, "[Error]逻辑服务器注册出错！缺少Key！\n");
		return -1;
	}
	if(key != G(r)->key) {
		kLOG(r, 0, "[Error]逻辑服务器注册出错！Key(%x != %x)错误！\n", key, G(r)->key);
		return -1;
	}
	if(rbtP_readInt(pkt, &id) < 0) {
		kLOG(r, 0, "[Error]逻辑服务器注册出错！缺少 ID！\n");
		return -1;
	}
	if(id < 0 || id >= LOGICAL_SERVER_MAX_NUM) {
		kLOG(r, 0, "[Error]逻辑服务器注册出错！ID(%d) 出错！Max:%d\n", id, LOGICAL_SERVER_MAX_NUM);
		return -1;
	}
	short int req_id;
	if(rbtP_readShort(pkt, &req_id) < 0) {
		kLOG(r, 0, "[Error]逻辑服务器注册出错！ID(%d) 缺少req_id！\n", id);
		return -1;
	}

	struct LogicalServer * serv = &G(r)->servs[id];
	if(serv->id > 0 || serv->c) {
		kLOG(r, 0, "[Error]逻辑服务器(%d)注册重复！把之前的断开！！\n", id);
		rbtRpc_call(r, NULL, serv->c, SERV_FOR_CONN_REG_AGAIN, "");
		// 将Group和PlayerList直接复用
	} else {
		list_init(&serv->player_list);
		serv->nuser = 0;
		serv->groups = rbtH_init(r, 0, 0);
		rbtH_weak(serv->groups);
	}

	rbtNet_set_authed(c, 1);

	struct ConnectionX * connX = rbtNet_get_x(c);
	connX->a1 = CONN_SERV;
	connX->a2 = id;

	serv->id = id;
	serv->c = c;

	serv->broadcast = array_create(r, sizeof(struct GlobalMessage), 16);
	serv->nbroadcast = 0;

	gettimeofday(&serv->come_tm, NULL);

	rbtStat_init(r, &serv->stat);

	rbtRpc_ret(r, c, req_id, "d", 1);

	return 0;
}

static int admin_reg(rabbit * r, Connection * c, Packet * pkt)
{
	rbtP_seek(pkt, 0);
	int key;
	if(rbtP_readInt(pkt, &key) < 0) {
		kLOG(r, 0, "[Error]Admin服务器注册出错！缺少Key！\n");
		return -1;
	}
	if(key != G(r)->key) {
		kLOG(r, 0, "[Error]Admin服务器注册出错！Key(%x != %x)错误！\n", key, G(r)->key);
		return -1;
	}
	short int req_id;
	if(rbtP_readShort(pkt, &req_id) < 0) {
		kLOG(r, 0, "[Error]Admin服务器注册出错！缺少Req id！\n");
		return -1;
	}

	kLOG(r, 0, "[LOG]Admin 服务器注册成功！\n");

	rbtNet_set_authed(c, 1);

	struct ConnectionX * connX = rbtNet_get_x(c);
	connX->a1 = CONN_ADMIN;

	G(r)->admin = c;

	rbtRpc_ret(r, c, req_id, "d", 1);

	return 0;
}

static int client_login(rabbit * r, Packet * pkt)
{
	rbtP_seek(pkt, 0);
	int account, zone_id;
	TString * session;
	
	if(rbtP_readString(pkt, &session) < 0) {
		kLOG(r, 0, "[Error]玩家登陆出错！缺少Session！");
		return -1;
	}
	if(rbtP_readInt(pkt, &account) < 0) {
		kLOG(r, 0, "[Error]玩家登陆出错！缺少Account！\n");
		return -1;
	}
	if(rbtP_readInt(pkt, &zone_id) < 0) {
		kLOG(r, 0, "[Error]玩家登陆出错！缺少ZoneID！\n");
		return -1;
	}

	// 验证...
	int v_account = get_account_from_open_ticket( (unsigned char*)(rbtS_gets(session)) );
	if( !v_account ) {
		// 验证失败！
		kLOG(r, 0, "[LOG]玩家登录验证失败！Session(%s), Account(%d) ZoneID(%d)\n", rbtS_gets(session), account, zone_id);
		return -1;
	}

	if( v_account != account) {
		// 验证成功，但账号不对
		kLOG(r, 0, "[LOG]玩家登陆，验证成功，但是账号不对？(%d) != (%d)\n", account, v_account);
		return -1;
	}
	rbtF_player_login(r, account, zone_id);
	return 0;
}

int rbtF_rpc_process( rabbit * r, Connection * c, int fun, Packet * pkt )
{
	struct ConnectionX * connX = rbtNet_get_x(c);
	if (!connX->a1) {
		if(fun == CONN_FOR_SERVER_REGISTER) {
			return serv_reg(r, c, pkt);
		}
		if(fun == CONN_FOR_ADMIN_REGISTER) {
			return admin_reg(r, c, pkt);
		}
	}

	if (connX->a1 == CONN_GATE) {
		int tmp = (fun >> 8) & 0x1F;
		if (tmp == CLIENT_LOGIN) {
			/// 1)注册 -- 注册包包尾没有 player->account 信息
			if(rbtP_size(pkt) > 1024) {
				kLOG(r, 0, "[Error]登录数据包过大(%d)!\n", rbtP_size(pkt));
				return -1;
			}
			return client_login(r, pkt);
		} else if (tmp == CONN_FOR_SERVER_REPORT_LOGOUT) {
			/// 2)玩家退出 -- 只包含一个 player->account 信息
			int acc = -1;
			if (rbtP_readInt(pkt, &acc) < 0) {
				kLOG(r, 0, "[Error] 读取退出的玩家 account 出错?\n");
				return 0;
			}
			struct Player * player = rbtF_player_get_account(r, acc);
			if (!player) {
				kLOG(r, 0, "[Error]玩家断线 账号(%d)不存在？\n", acc);
				return 0;
			}
			rbtF_player_logout(r, acc);
			return 0;
		} else {
			/// 3)RPC调用 -- 非注册包尾都会添加一个 player->account 信息
			G(r)->stat.npkt_recv++;
			G(r)->stat.recv_size += rbtP_size(pkt);
			struct Player * ply = rbtF_player_get_account(r, connX->a2);
			if(!ply) {
				kLOG(r, 0, "[Error]链接发送数据，关联玩家（%d），不存在！\n", connX->a2);
				return -1;
			}
			ply->stat.npkt_recv++;
			ply->stat.recv_size += rbtP_size(pkt);
			if(ply->state != E_PlayerState_OK) {
				kLOG(r, 0, "[Error]玩家(account:%d)发送数据，但是这个玩家不是OK状态！\n", connX->a2);
				return 0;
			}
			return rbtF_rpc_client(r, c, fun, pkt);
		}
		return 0;
	}

	if (connX->a1 == CONN_ADMIN) {
		if(c == G(r)->admin) {
			rbtF_rpc_admin(r, c, fun, pkt);
		}
		return 0;
	}

	if (connX->a1 == CONN_SERV) {
		struct LogicalServer * serv = rbtF_get_serv(r, connX->a2);
		if(!serv) {
			kLOG(r, 0, "[Error]链接发送数据，关联服务器（%d），不存在！\n", connX->a2);
			return -1;
		}
		serv->stat.npkt_recv++;
		serv->stat.recv_size += rbtP_size(pkt);

		return rbtF_rpc_server(r, c, fun, pkt);
	}

	return 0;
}

