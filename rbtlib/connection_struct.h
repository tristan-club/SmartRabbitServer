#ifndef connection_struct_h_
#define connection_struct_h_

#include "list.h"
#include "buffer.h"
#include "queue.h"
#include "pool.h"
#include "connectionX.h"

#define MAX_PACKET_SEQ	10

struct Connection {
	rabbit * r;

	// 所有的 Connection 链在一起
	struct list_head list;

	int fd;

	int status;

	// 创建的时间
	time_t  time;

	// 没有验证的 Connection 链在一起，10秒后清除
	int authed;
	struct list_head unauthed_list;

	// Read & Write
	Buffer * read_buffer;
	Queue * write_queue;

	// X
	struct ConnectionX x;

	// Rpc
	struct list_head rpc_param;

	// 统计
	int recv_nmem;
	int recv_npkt;
	int sent_nmem;
	int sent_npkt;

	// IP/ Port
	char ip[16];
	int port;

	// 发包计数
	short int sent_seq;
	short int recv_seq_expect;

	// 这个链接上发包，是否需要加密
	int is_encode;

	// close的时候，是否是主动close，主动close不进行回调
	int active_close;
};


#endif

