#include "twice_login.h"
#include "player.h"
#include "svr_connection.h"
#include "server.h"

static void real_login(rabbit * r, int account, int zone_id)
{
	struct Player * ply = rbtF_player_init(r, account);
	ply->zone_id = zone_id;
	ply->state = E_PlayerState_OK;
	struct LogicalServer * serv = rbtF_get_serv_by_zone(r, zone_id);
	if(!serv) {
		kLOG(r, 0, "[LOG]玩家登陆，(Account:%d)登陆，没有这个服务器！ZoneID(%d) \n", account, zone_id); 
		return;
	}
	struct Zone * zone = rbtF_get_zone(r, zone_id);
	if(!zone) {
		kLOG(r, 0, "[LOG]玩家登陆，(Account:%d)登陆，没有这个Zone！ZoneID(%d)\n", account, zone_id);
		return;
	}
	zone->nuser++;
	serv->nuser++;
	G(r)->nuser++;

	Packet * pkt_t = rbtP_init(r);
	rbtP_set_fun(pkt_t, SERVER_ENTER_ZONE | PKT_FUN_MASK);
	rbtP_writeInt(pkt_t, account);
	rbtP_writeInt(pkt_t, zone_id);
	rbtF_send_serv(r, serv->id, pkt_t);
	rbtP_drop(pkt_t);

	kLOG(r, 0, "[LOG]玩家登陆成功 Account(%d), ZoneID(%d) Zone总人数:%d, ServID(%d) Serv总人数:%d，总人数:%d\n", account, zone_id, zone->nuser, serv->id, serv->nuser, G(r)->nuser);
}

static int report_logout(rabbit * r, struct Player * ply)
{
	struct Zone * zone = rbtF_get_zone(r, ply->zone_id);
	if(!zone) {
		kLOG(r, 0, "[Error] 玩家(account:%d)退出，报告给服务器，但是zone(%d)不存在！\n", ply->account, ply->zone_id);
		return -1;
	}
	zone->nuser--;
	struct LogicalServer * serv = rbtF_get_serv(r, zone->serv_id);
	if(!serv) {
		kLOG(r, 0, "[Error] 玩家(account:%d)退出，报告给服务器, zone:%d，但是server(%d)不存在！\n", ply->account, ply->zone_id, zone->serv_id);
		return -1;
	}
	serv->nuser--;

	kLOG(r, 0, "[Error] 玩家(account:%d)退出，报告给服务器, zone:%d，server(%d)！\n", ply->account, ply->zone_id, zone->serv_id);

	Packet * pkt = rbtP_init(r);
	rbtP_set_fun(pkt, SERVER_PLAYER_ABORT | PKT_FUN_MASK);
	rbtP_writeInt(pkt, ply->account);
	rbtF_send_serv(r, serv->id, pkt);
	rbtP_drop(pkt);

	return 0;
}

static void twice_login(rabbit * r, struct Player * ply)
{
	int account = ply->account;
	int zone_id = ply->zone_id;

	// 1. 完全释放之前的Player
	rbtF_player_destroy(r, ply);

	// 2. 重新登录一个
	real_login(r, account, zone_id);
}

void rbtF_player_login(rabbit * r, int account, int zone_id)
{
	struct Player * ply = rbtF_player_get_account(r, account);

	if (!ply) {
		// 玩家第一次登录，直接登录好了
		real_login(r, account, zone_id);
		return;
	}

	// 还没退出成功，就登录了
	if(ply->state == E_PlayerState_Saving || ply->state == E_PlayerState_Twice) {
		ply->state = E_PlayerState_Twice;
		ply->account = account;
		ply->zone_id = zone_id;

		struct timeval tm;
		gettimeofday(&tm, NULL);
		if(tm.tv_sec - ply->come_tm.tv_sec > 15) {
			// 15 秒后，还没保存返回，视为保存成功
			kLOG(r, 0, "[Warning]玩家(account:%d) 退出，15秒还没有保存成功返回，视为已经保存成功！进行二次登录！\n", account);
			twice_login(r, ply);
		}
		return;
	}

	// 2次登录

	// 释放各种链接等
	rbtF_player_abort(r, ply);

	gettimeofday(&ply->come_tm, NULL);

	// 记录新的值
	ply->state = E_PlayerState_Twice;
	ply->account = account;

	// 报告给服务器，玩家退出
	if(report_logout(r, ply) < 0) {
		// 报告失败，直接登录
		kLOG(r, 0, "[Warning]玩家(account:%d) 2次登录，给相关服务器发送数据失败！进行二次登录！\n", account);
		ply->zone_id = zone_id;
		twice_login(r, ply);
		return;
	}

	kLOG(r, 0, "[Warning]玩家(account:%d) 2次登录，等待服务器保存用户数据。。。\n", account);
	ply->zone_id = zone_id;
}

/*
 * 玩家断线
 *
 */
void rbtF_player_logout(rabbit * r, int account)
{
	struct Player * ply = rbtF_player_get_account(r, account);
	if(!ply) {
		kLOG(r, 0, "[Error] 玩家(account:%d)断线，没找到这个玩家！\n", account);
		return;
	}

	if(ply->state != E_PlayerState_OK && ply->state != E_PlayerState_Twice) {
		return;
	}

	// 报告给服务器，玩家退出(OK状态才报告，Twice状态不需要，因为之前已经汇报一次了)
	if(ply->state == E_PlayerState_OK) {
		report_logout(r, ply);
	}

	ply->state = E_PlayerState_Saving;

	// 从各种组、链表里退出
	rbtF_player_abort(r, ply);

	kLOG(r, 0, "[Error] 玩家(account:%d)断线，等待服务器保存玩家数据。。。\n", account);
}

void rbtF_player_save_ok(rabbit * r, int account)
{
	struct Player * ply = rbtF_player_get_account(r, account);
	if(!ply) {
		kLOG(r, 0, "[Error] 玩家(account:%d)Save OK，没找到这个玩家！\n", account);
		return;
	}

	if(ply->state == E_PlayerState_Saving) {
		kLOG(r, 0, "[LOG] 玩家(account:%d, pid:%d) Save OK! 释放最后内存！\n", account, ply->pid);
		rbtF_player_destroy(r, ply);
		return;
	}

	if(ply->state == E_PlayerState_Twice) {
		kLOG(r, 0, "[LOG] 玩家(account:%d, pid:%d) Save OK. 继续二次登录！\n", account, ply->pid);
		twice_login(r, ply);
		return;
	}

	kLOG(r, 0, "[Error] 玩家(account:%d, pid:%d) Save OK. 玩家状态不对(%d)!\n", account, ply->pid, ply->state);
}

