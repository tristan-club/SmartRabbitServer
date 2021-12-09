#ifndef twice_login_h_
#define twice_login_h_

#include "server.h"

// 玩家登录时，调用
void rbtF_player_login(rabbit * r, int account, int zone_id);

// 玩家断线时，调用
void rbtF_player_logout(rabbit * r, int account);

// 玩家数据保存结束后，调用
void rbtF_player_save_ok(rabbit * r, int account);

#endif

