#ifndef connection_h_
#define connection_h_

#include "rabbit.h"
#include "object.h"

struct Connection;
struct ConnectionX;

/*
 *	连接的状态有3种:
 *	
 *	(1) 正在连接(C)，这个时候这个连接不能发送任何数据，任何尝试发送的数据都会被丢弃（Deprecated! 这个状态的存在，会让写应用和写库的都很吃力）
 *
 *	(2) 连接确定(E), 处于连通状态，可以收发数据
 *
 *	(3) 僵尸状态(Z), 连接不再收发新数据，当把储存待发的数据全部发送成功后，连接正式关闭
 *
 *	(4) 关闭状态(E), 连接不能通讯了，不过还不能立即回收，需要告诉上层逻辑（调用一个回调函数）某一个连接关闭了，当回调函数返回时，连接正式回收
 */
enum {
//	CONN_CONNECTING,
	CONN_ESTABLISHED,
	CONN_ZOMBIE,
	CONN_CLOSED
};

/*
 *	获取/修改连接的状态
 *
 *	@param c
 */
int rbtNet_status( struct Connection * c );

int rbtNet_set_status( struct Connection * c, int status );

/*
 *	获取 fd
 *
 *	@param c
 */
int rbtNet_fd( struct Connection * c );

const char * rbtNet_ip( struct Connection * c );
int rbtNet_port( struct Connection * c );

/*
 *	连接是否有数据待写
 *
 *	@param c
 */
int rbtNet_empty( struct Connection * c );

/*
 *	向一个连接发送一个数据包
 *
 *	@param r
 *	@param c
 *	@param pkt
 */
int rbtNet_send( rabbit * r, struct Connection * c, const Packet * pkt );

int rbtNet_send_weak( rabbit * r, struct Connection * c, const Packet * pkt );


/*
 *	处理 网络 读/写 事件，并且分离出来数据包，转入RPC调用
 *
 *	@param c
 *	@param event
 */
int rbtNet_process( struct Connection * c, int event );

/*
 *	新建一个连接，这个函数只在一个新连接确认后，由‘连接管理者(fdevent_handler)’调用
 *
 *	@param r
 *	@param fd
 */
struct Connection * rbtNet_construct( rabbit * r, int fd );

void rbtNet_conn_free(struct Connection * c);


/*
 *	取得 Connectin 的 eXtension
 *
 *	@param c
 */
struct ConnectionX * rbtNet_get_x( struct Connection * c );


/*
 *	Connection 是否验证过
 *
 */
int rbtNet_is_authed(struct Connection * c);
int rbtNet_set_authed(struct Connection * c, int auth);

int rbtNet_is_encode(struct Connection * c);
void rbtNet_set_encode(struct Connection * c, int encode);

/*
 *	Dump 出一个连接的信息
 *
 *	@param c
 */
int rbtD_conn( struct Connection * c );
int rbtD_conn_mem(struct Connection * c );

int rbtNet_conn_count();
int rbtNet_conn_mem();

#endif

