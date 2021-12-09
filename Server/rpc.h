#ifndef rpc_h_
#define rpc_h_

/*
 *	连接服务器暴露给外面的 RPC 调用
 *
 *	#define CLIENT_LOGIN            1
 *	#define CLIENT_GET              2
 *	#define CLIENT_SET              3
 *	#define CLIENT_GROUP_MSG        4
 *	#define CLIENT_BUDDY_MSG        5
 *	#define CLIENT_APPLY            6               // 客户端向连接服务器请求服务, 连接服务器只将这个请求转发给适当的逻辑服务器
 *	#define CLIENT_GROUP_CALL       7               // 客户端调用组里特定函数
 *	#define CLIENT_GROUP_ENTER      8
 *	#define CLIENT_GROUP_LEAVE      9
 *
 *	#define SERVER_REGISTER                 1001
 *	#define SERVER_GROUP_MSG                1002
 *	#define SERVER_BUDDY_MSG                1003
 *	#define SERVER_GROUP_CREATE             1004
 *	#define SERVER_GROUP_DESTROY            1005
 *	#define SERVER_GROUP_ADD                1006
 *	#define SERVER_GROUP_DROP               1007
 *
 */

#include "server.h"

/*
 *	RPC 总入口
 *
 *	@param r
 *	@param c
 *	@param fun
 *	@param pkt
 */
int rbtF_rpc_process( rabbit * r, Connection * c, int fun, Packet * pkt );


// For Client

int rbtF_rpc_client( rabbit * r, Connection * c, int fun, Packet * pkt );


// For Server
int rbtF_rpc_server( rabbit * r, Connection * c, int fun, Packet * pkt );

// For Admin
int rbtF_rpc_admin( rabbit * r, Connection * c, int fun, Packet * pkt );

#endif

