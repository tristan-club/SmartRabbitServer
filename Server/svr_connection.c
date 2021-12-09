#include "svr_connection.h"
#include "player.h"
#include "boot.h"
#include "twice_login.h"

int rbtF_conn_broken( rabbit * r, Connection * c )
{
	struct ConnectionX * connX = rbtNet_get_x(c);

	if(c == G(r)->admin) {
		kLOG(r, 0, "[LOG]Admin 服务器退出！\n");
		G(r)->admin = NULL;
		return 0;
	}

	int id = connX->a2;
	if (connX->a1 == CONN_SERV) {
		kLOG(r, 0, "[LOG]逻辑服务器退出，ID: %d\n", id);
		struct LogicalServer * serv = rbtF_get_serv(r, id);
		if(!serv) {
			kLOG(r, 0, "[Error]逻辑服务器不存在？\n");
			return 0;
		}
		if(serv->c != c) {
			kLOG(r, 0, "[Error]逻辑服务器断开，但是c(%p) != serv->c(%p) ？？？\n", c, serv->c);
			return 0;
		}
		serv->id = -1;
		serv->c = NULL;
		serv->groups = NULL;

		// 将所有用户去掉
		int loop = 60000;	// 防止死循环，最多60000次
		struct list_head *p, *tmp;
		list_foreach_safe(p, tmp, &serv->player_list) {
			list_del(p);
			loop--;
			if(loop <= 0) {
				kLOG(r, 0, "[Error] 逻辑服务器断开，将所有用户去掉,%s:%d，已经死循环！\n", __FUNCTION__, __LINE__);
				break;
			}
		}
		list_init(&serv->player_list);
		return 0;
	}

	if (connX->a1 == CONN_GATE) {
		return boot_gate(r);
	}

	return 0;
}

