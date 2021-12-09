#ifndef fdevent_h_
#define fdevent_h_

#include "object.h"

struct NetManager;

#define EVENT_READ	1
#define EVENT_WRITE	2
#define EVENT_HUP	4
#define EVENT_ERR	8

/*
 *	初始化网络层，必须在其他网络函数调用之前调用
 *
 *	@param r
 */
int rbtNet_init( rabbit * r );

/*
 *	设置全局唯一的监听端口 -- 一个程序只需要有一个监听端口就可以了，多个只会徒增复杂性
 *
 *	@param r
 *	@param port
 */
int rbtNet_listen( rabbit * r, int port );


/*
 *	等待网络事件
 *
 *	@param r
 *	@param milsec	--	等待的最长时间（微秒）
 */
int rbtNet_poll( rabbit * r, time_t milsec );



/*
 *      阻塞连接，连接成功后才返回(会循环连接，一直到成功)
 *
 *      @param r
 *      @param address
 *      @param port
 */
struct Connection * rbtNet_connect_try_hardly( rabbit * r, int address, int port );

struct Connection * rbtNet_connect_try_once( rabbit * r, int address, int port );


/*
 *      关闭一个连接，连接不会立即关闭，当连接上所有待发送的数据发送成功后才关闭
 *			连接不会再收到任何数据
 *
 *      @param r
 *      @param c
 */
int rbtNet_close( struct Connection * c );

// Debug
int rbtNet_mgr_count();
int rbtNet_mgr_mem();


#endif

