#ifndef remote_call_h_
#define remote_call_h_

#include "rabbit.h"

#define RPC_REQ_ID_NO   0


/*
 *	初始化 RPC，必须在其他 RPC 函数调用之前调用
 *
 *	@param r
 */
int rbtRpc_init( rabbit * r );

void rbtRpc_enable(rabbit * r, int b);


/*
 *	RPC 调用统一的参数格式
 *
 *	*_id* 为内部数据，不可更改
 *
 *
 */
struct RpcParam {
	union {
		int i;
		unsigned int u;
		void * p;
	};
	GCObject * obj;
	GCObject * obj2;
};

/*
 *	RPC 调用
 *
 *	@param r
 *	@param f	-- 回调函数，形式如下: f(rabbit * r, struct Connection * c, struct RpcParam * param, Packet * pkt)；如果不期待回调，设为NULL即可
 *	@param c	-- RPC 调用的连接
 *	@param fun_id	-- RPC 调用远程函数
 *	@param fmt	-- 参数格式
 *	@param ...	-- 由参数格式所描述的参数
 *
 *	@return		-- 返回一个指向 struct RpcParam 的指针，可以填入参数，在回调的时候会以参数形式出现
 */
struct RpcParam * rbtRpc_call( rabbit * r, void * f, Connection * c, int fun_id, const char * fmt, ... );


/*
 *	和 rbtRpc_call 一样，也是 RPC 调用，不过 要发送的 Packet 已经准备好了
 *
 *	@param r
 *	@param f	-- 回调函数
 *	@param c	-- RPC 调用的连接
 *	@param fun_id	-- RPC 调用的远程函数
 *	@param pkt	-- 已经填好参数的 数据包
 */
struct RpcParam * rbtRpc_packet( rabbit * r, void * f, Connection * c, int fun_id, Packet * pkt );


/*
 *	RPC 调用返回
 *
 *	@param r
 *	@param c	-- 由此连接返回
 *	@param req_id	-- 请求序列号
 *	@param pkt	-- 返回的数据包
 */
int rbtRpc_apply( rabbit * r, Connection * c, size_t req_id, Packet * pkt );


/*
 *	针对于某一个 RPC 调用返回结果
 *
 *	@param r
 *	@param c
 *	@param req_id	-- RPC 请求序列号
 *	@param fmt	-- 返回的参数格式
 *	@param ...	-- 由参数格式描述的参数
 */
int rbtRpc_ret( rabbit * r, Connection * c, int req_id, const char * fmt, ... );



/*
 *	当一个 Connection 断开连接时，应将在其身上等待的 rpc 调用全部删除
 *
 *	@param r
 *	@param c
 */
void rbtRpc_conn_broken( rabbit * r, Connection * c );

/*
 *	垃圾回收标记
 *
 *	@param r
 *	@param mgr
 */
void rbtRpc_traverse( rabbit * r, RpcManager * mgr );

int rbtRpc_debug_mem();
int rbtRpc_debug_param_count();
int rbtRpc_debug_param_mem();

#endif

