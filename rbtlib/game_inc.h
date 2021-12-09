#ifndef rpc_type_h_
#define rpc_type_h_

/* 服务器类型 */
#define SERVER_TYPE_WORLD		1	// 逻辑服务器（世界服务器）
#define SERVER_TYPE_KNIGHT		2	// 骑士团服务器
#define SERVER_TYPE_CHAT		3	// 聊天服务器
#define SERVER_TYPE_COPY		4	// 副本服务器
#define SERVER_TYPE_MAX			5


/* 调用客户端的RawFun */
#define RAW_PLAYER_LEAVE_SCENE		1
#define RAW_RT_UPDATE_NPC_PHY		30

/* 连接服务器*/

// for client
#define CLIENT_LOGIN			1	//登录
#define CLIENT_ENTER_ZONE		2	//进入Zone

#define CLIENT_KEEP_ALIVE		3	// 客户端保存连接发的包，原包发回

#define CLIENT_CALL_BUDDY		7	//单传

#define CLIENT_CALL_SERVER_SYSTEM	8	//调用服务器 system 函数
#define CLIENT_CALL_SERVER_SCENE	9	//调用服务器 当前组 的函数
#define CLIENT_CALL_SERVER_OTHER	12	//

#define CLIENT_CALL_GROUP		10	//广播(当前组)
#define CLIENT_CALL_WORLD_MSG		11	//世界广播（当前逻辑服务器）
#define CLIENT_ROBOT_CALL_GROUP		13	//广播(当前组)


// for server 
#define CONN_FOR_SERVER_REGISTER			101	// 逻辑服务器注册
#define CONN_FOR_SERVER_BUDDY_CALL			102	// 调用客户端方法
#define CONN_FOR_SERVER_BUDDY_CALL_RAW			109	// 调用客户端方法

#define CONN_FOR_SERVER_GROUP_CALL			103	// 在一个组里广播
#define CONN_FOR_SERVER_GROUP_CALL_RAW			111	// 在一个组里广播
#define CONN_FOR_SERVER_GROUP_CALL_RAW_BUT		113	// 在一个组里广播，但是后面跟一个忽略的pid

#define CONN_FOR_SERVER_BUDDY_BIND			104	// 一个账号和一个角色绑定
#define CONN_FOR_SERVER_ADD_PLAYER_TO_GROUP		105	// 将一个玩家加到一个组里
#define CONN_FOR_SERVER_REMOVE_PLAYER_FROM_GROUP	106	// 将一个玩家从一种组里删除 -- 当一个组里没有玩家时，自动删除
#define CONN_FOR_SERVER_CREATE_ZONE			107	// 创建一个 Zone
#define CONN_FOR_SERVER_GLOBAL_MESSAGE			108	// 全局消息

#define CONN_FOR_SERVER_STATISTIC			110	// Statistic 统计

#define CONN_FOR_SERVER_BUDDY_ABORT_OK			112	// 玩家成功退出（数据保存成功）

#define CONN_FOR_ADMIN_REGISTER				201	// Admin注册
#define CONN_FOR_ADMIN_STATISTIC			202	// 统计





/* 逻辑服务器 */

#define SERVER_ENTER_ZONE			3	// 用户进入Zone

#define SERVER_LEAVE_GROUP			5	// 用户离开组

#define SERVER_CALL				7	// 调用服务器函数
#define SERVER_CALL_SCENE			8	// 调用服务器函数
#define SERVER_CALL_OTHER			9

#define SERVER_PLAYER_ABORT			13	// 用户断线！

#define SERV_FOR_CLIENT_UPDATE_PLY_PHY		100	// 玩家更新物理
#define SERV_FOR_CLIENT_ATTACK_MONSTER		101	// 玩家打怪
#define SERV_FOR_CLIENT_START_PUSH		102
#define SERV_FOR_CLIENT_END_PUSH		103
#define SERV_FOR_CLIENT_ADD_BUFF		104	// 给怪加Buff

#define SERV_FOR_CENTER_GET_UDATA		200	// center 服务器调用，获取一个玩家的数据
#	define UD_REMOTE_INFO		0
#	define UD_REMOTE_ITEM		1
#	define UD_REMOTE_MAX		2

#define SERV_FOR_CENTER_LOGIN_AGAIN		201	// center 服务器调用，某个玩家2次登录
#define SERV_FOR_CENTER_CALL			202	// 一个服务器被另一个服务器call

#define SERV_FOR_CONN_REG_AGAIN			300	// 连接服务器让逻辑服务器退出，通常在有另一个逻辑服务器把这个挤下去




/* 中央服务器 */
#define CENTER_FOR_SERV_REGISTER		1	// 服务器注册
#define CENTER_FOR_SERV_PLY_ENTER		2	// 玩家进入
#define CENTER_FOR_SERV_PLY_LEAVE		3	// 玩家离开
#define CENTER_FOR_SERV_GET_UDATA		4	// 获取玩家数据
#define CENTER_FOR_SERV_GET_UDATA_BACK		5	// 获取玩家数据回调

#define CENTER_FOR_SERV_CALL			6	// 一个服务器，call 另外一个服务器的函数（以玩家定位）
#define CENTER_FOR_SERV_CALL_BACK		7	// 一个服务器，call 另外一个服务器的函数 返回
#define CENTER_FOR_SERV_CALL_2			8	// 一个服务器，call 另外一个服务器的函数2(以服务器type：id定位）


#define CENTER_FOR_ADMIN_REGISTER		200	// Admin 注册




/* 数据服务器暴露给其他的 RPC 调用 */
#define DATA_CLIENT_LOGIN			1	// 客户端登录
#define DATA_SERVER_LOGIN			2	// 服务器登录

#define DATA_PLAYER_GET_INFO			10	// 得到用户的信息（客户端或服务器）
#define DATA_PLAYER_GET_INFO_ID			10	// 通过id得到用户信息
#define DATA_PLAYER_GET_INFO_NAME		11	// 通过用户名得到用户信息
#define DATA_PLAYER_GET_INFO_NICKNAME		12	// 通过用户昵称得到用户信息
#define DATA_PLAYER_GET_INFO_EMAIL		13	// 通过用户email得到用户信息

#define DATA_PLAYER_GET_ITEM_ID			20	// 通过id得到用户装备信息

#define DATA_PLAYER_ITEM_EQUIP			21	// 保存用户装备信息

#define DATA_PLAYER_CLEAR_CACHE			22

#define DATA_SAVE_NOTICE			28
#define DATA_PLAYER_SAVE_SYS			29
#define DATA_PLAYER_SAVE_INFO			30	// 保存用户的信息（必须是服务器）

#define DATA_FRIEND_LIST			31	// 获取好友信息

#define DATA_ITEM_GET				40	// 获取单个装备信息

#define DATA_SCENE_GET_INFO			100	// 得到场景的信息（客户端或服务器）
#define DATA_SCENE_GET_NPC_INFO			101
#define DATA_SCENE_SAVE_INFO			200	// 保存场景的信息（必须是服务器）

#define DATA_DEL_BY				990
#define DATA_DELETE				991
#define DATA_GET_NUM				992
#define DATA_UPDATE				995	// 更新
#define DATA_GET_BY				996
#define DATA_GET				997
#define DATA_GET_ALL				998
#define DATA_SAVE				999	// 保存、插入

#define DATA_RESULT_NEED_UPDATA			1000	// 需要更新
#define DATA_RESULT_UP_TO_DATE			1001	// 不需要更新
#define DATA_RESULT_NOTHING			1002	// 请求的结果不存在
#define DATA_RESULT_BAD_PARAM			1003	// 提供的参数不正确

#define DATA_SAVE_OK				2000	// 保存成功
#define DATA_SAVE_UNIQUE_CONFLICT		2001	// unique key conflict
#define DATA_SAVE_ERROR				2002	// 保持出错

#define DATA_DEL_OK				2010    // 删除成功
#define DATA_DEL_ERROR				2011    // 删除失败





/* 聊天服务器接口 */
#define CHAT_FOR_ADMIN_REG			0	// Admin 登录
#define CHAT_LOGIN				1	// 用户登录
#define CHAT_INVOKE_BUDDY			2	// 用户聊天
#define CHAT_CALL_SERVER			3	// 调用服务器函数



/* 副本服务器 */
#define COPY_S_LOGIN				1
#define COPY_S_ENTER_ZONE			2

#define COPY_C_LOGIN_SUCC			1
#define COPY_C_LOGIN_FAIL			2
#define COPY_C_ENTER_ZONE_SUCC			3
#define COPY_C_ENTER_ZONE_FAIL			4

#endif

