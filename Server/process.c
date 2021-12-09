#include "process.h"
#include "player.h"

static void broadcast_at(rabbit * r, struct LogicalServer * serv);

void process(rabbit * r)
{
	int i;
	for(i = 0; i < LOGICAL_SERVER_MAX_NUM; ++i) {
		struct LogicalServer * serv = &G(r)->servs[i];
		if(serv && serv->id > 0 && serv->c) {
			broadcast_at(r, serv);
		}
	}
}

static void broadcast_at(rabbit * r, struct LogicalServer * serv)
{
	if(serv->nbroadcast <= 0) {
		return;
	}

	int i, c;
	struct Player * ply;
	struct list_head * curr;
	for(i = 0; i < array_length(serv->broadcast); ++i) {
		struct GlobalMessage * gm = array_at(serv->broadcast, i);
		if(gm->pkt) {
			for(c = 10, curr = gm->next; (curr != &serv->player_list) && (c > 0); curr = curr->next, c--) {
				ply = list_entry(curr, struct Player, player_list);
				rbtF_send_player(r, ply, gm->pkt);

				kLOG(r, 0, "[LOG]把系统消息发给：%d\n", ply->pid);
			}
			if(curr == &serv->player_list) {
				// 发完了
				rbtP_drop(gm->pkt);
				gm->pkt = NULL;
				serv->nbroadcast--;
			}
		}
	}
}

