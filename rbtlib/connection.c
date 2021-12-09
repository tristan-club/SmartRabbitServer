#include "connection.h"

#include "object.h"

#include "mem.h"
#include "gc.h"
#include "buffer.h"

#include "packet.h"
#include "packet_struct.h"
#include "mblock.h"

#include "table.h"
#include "queue.h"
#include "pool.h"

#include "net_manager.h"

#include "remote_call.h"

#include "connection_struct.h"

static int read_data( struct Connection * c );

static int write_data( struct Connection * c );

// 10K 个链接
#define MAX_CONN_NR 10240

static struct Connection g_Conn[MAX_CONN_NR];
static struct list_head g_FreeConn;
static int g_ConnInit = 0;

// debug
static int g_nConn = 0;
int rbtNet_conn_count()
{
	return g_nConn;
}

int rbtNet_conn_mem()
{
	return g_nConn * sizeof(struct Connection);
}

inline struct ConnectionX * rbtNet_get_x( struct Connection * c )
{
	return &c->x;
}

inline int rbtNet_status( struct Connection * c )
{
	return c->status;
}

inline int rbtNet_set_status( struct Connection * c, int status )
{
	if(c->status == status) {
		return 0;
	}
	if(c->status == CONN_CLOSED) {
		// 已经关闭的链接，不能做其他事情
		assert(0);
		return 0;
	}
	if(c->status == CONN_ZOMBIE) {
		// CONN_ZOMBIE 只能转向 CONN_CLOSED
		assert(status == CONN_CLOSED);
		return 0;
	}
	if(c->status == CONN_ESTABLISHED) {
		// CONN_ESTABLISHED 只能转向 CONN_ZOMBIE
		assert(status == CONN_ZOMBIE);
		return 0;
	}
	return (c->status = status);
}

inline int rbtNet_fd( struct Connection * c )
{
	return c->fd;
}

inline const char * rbtNet_ip( struct Connection * c )
{
	return c->ip;
}

inline int rbtNet_port( struct Connection * c )
{
	return c->port;
}

inline int rbtNet_empty( struct Connection * c )
{
	return rbtQ_empty(c->write_queue);
}

inline void rbtNet_set_encode(struct Connection * c, int encode)
{
	c->is_encode = encode;
}

inline int rbtNet_is_encode(struct Connection * c)
{
	return c->is_encode;
}


static int read_data( Connection *c )
{
	rabbit * r = c->r;

	int nread = 0;
	if(ioctl(c->fd, FIONREAD, &nread) || nread == 0) {
		kLOG(r, 0, "[Error]Ioctl : %d\n", nread);
		perror("read ioctl");
		return -1;
	}

	Buffer * b = c->read_buffer;
	if(unlikely(!b)) {
		kLOG(r, 0, "[Error] %s : connection 没有read_buffer!(%d)\n", __FUNCTION__, rbtNet_status(c));
		return -1;
	}
	buffer_prepare_append( r, b, nread );

	int nrcv = 0;
	while( nread > 0 ) {
begin:
		nrcv = read( c->fd, b->p + b->used, b->size - b->used );
		if( nrcv < 0 ) {
			switch ( errno ) {
				case EINTR:
					goto begin;	// 信号中断, 可以继续读

				case EAGAIN:
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
				case EWOULDBLOCK:
#endif
					return 0;	// 读完了

				default:
					kLOG(r, 0, "[Error]Read(), Unknown Error(%d)\n", errno);
					perror("net::read");
					return -1;	// 未知错误, 返回错误
			}
		}
		c->recv_nmem += nrcv;

		nread -= nrcv;
		b->used += nrcv;
	}

	return 0;
}

static Packet * get_next_packet( Connection * c )
{
	rabbit * r = c->r;

	Buffer * b = c->read_buffer;

	if(unlikely(!b)) {
		kLOG(r, 0, "[Error] %s: connection没有read_buffer!(%d)\n", __FUNCTION__, rbtNet_status(c));
		return NULL;
	}

	if(b->pos >= b->used) {
		b->pos = b->used = 0;
		return NULL;
	}

	/* 从上一个包的后一字节开始分析 */
	Packet * pkt = rbtP_parse( r, b->p + b->pos, b->used - b->pos );

	if( !pkt ) {
		if ( r->redis == c ) {
			// 1. 更改read_buffer大小为适当大小
			// 2. 维护read_buffer的pos used size
			b->used = 0;
			b->pos = 0;
		} else {
			memmove(b->p, b->p + b->pos, b->used - b->pos);

			b->used = b->used - b->pos;
			b->pos = 0;
		}
	} else {
		b->pos += rbtP_size(pkt);

		c->recv_npkt++;
	}

	return pkt;
}

struct PacketNode {
	const Packet * pkt;
	struct MBlock * mcurr;
	char * pcurr;
	char * pend;

	int left;
	int gid;

	short int pkt_seq;
};

static int write_data( Connection * c )
{
	rabbit * r = c->r;
	used(r);

	if(unlikely(!c->write_queue)) {
		kLOG(r, 0, "[Error]%s:status(%d)没有write_queue!\n", __FUNCTION__, rbtNet_status(c));
		return -1;
	}

	while( !rbtQ_empty(c->write_queue) ) {
		struct PacketNode * pn = cast(struct PacketNode *, rbtQ_peek(c->write_queue));
		if(!pn || !pn->pkt) {
			rbtQ_pop(c->write_queue);
		       	continue;
		}
		Packet * pkt = cast(Packet *, pn->pkt);
		if(pkt != rbtP_FlashSecurityPacket && pkt != rbtP_HugePacket) {
			if (PKT_IS_HEAD_ENCODE(rbtP_get_flag(pkt))) {
				rbtP_head_decode(pkt);
			}

			rbtP_set_seq(pkt, pn->pkt_seq);

			if (rbtNet_is_encode(c)) {
				rbtP_head_encode(pkt);
				rbtP_encode(pkt);
			}
		}

		int nsend = 0;

		int sz = rbtP_size(pkt);
		if(sz > 500 * 1024) {
			kLOG(r, 0, "[BIG_PACKET] start send : %d\n", sz);
		}

		while( pn->left > 0 ) {
begin:
			nsend = write(c->fd, pn->pcurr, min(pn->pend - pn->pcurr, pn->left));
			if(nsend == -1) {
				switch ( errno ) {
					case EAGAIN:
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
					case EWOULDBLOCK:	// 系统缓存已满, 下次再写
#endif
						goto end;

					case EPIPE:
						kLOG(r, 0, "[Error] %s : %s, EPIPE!\n", __FILE__, __FUNCTION__);
						return -1;

					case EINTR:
						goto begin;	// 信号中断, 可以继续写

					default:
						return -1;
				}
			}

			if(sz > 500 * 1024) {
				kLOG(r, 0, "[BIG_PACKET] size:%d, sent : %d\n", sz, nsend);
			}

			c->sent_nmem += nsend;

			pn->left -= nsend;
			if(pn->left <= 0) {
				break;
			}
			pn->pcurr += nsend;
			if(pn->pcurr >= pn->pend) {
				struct MBlock ** pnext = mblock_next(pn->mcurr);
				if(!(*pnext)) {
					pn->left = 0;
					break;
				}
				pn->mcurr = *pnext;
				pn->pcurr = cast(char *, pn->mcurr);
				pn->pend = cast(char *, mblock_next(pn->mcurr));
			}
		}

		if(pn->left <= 0) {
			int sz = rbtP_size(pkt);
			if(sz > 500 * 1024) {
				kLOG(r, 0, "[BIG_PACKET] Packet size:%d, sent completed!\n", sz);
			}
			rbtP_drop( cast(Packet *, pkt) );

			rbtQ_pop(c->write_queue);
		}
	}
end:
	return 0;
}

int rbtNet_process( struct Connection * c, int e )
{
	rabbit * r = c->r;

	if((e & EVENT_READ) && (c->status == CONN_ESTABLISHED)) {
		if( read_data( c ) < 0 ) {
			kLOG(r, 0, "[Error]read data error.fd:%d.\n",c->fd);
			return -1;
		}
		Packet * pkt;
		while( (pkt = get_next_packet(c)) ) {

			if(pkt == rbtP_FlashSecurityPacket) {
				rbtNet_send(r, c, rbtP_FlashSecurityPacket);
				kLOG(r, 0,  "[LOG]close flash security packet, ip:%s, port:%d\n", rbtNet_ip(c), rbtNet_port(c));
			//	rbtNet_close(c);

				break;
			}

			if(pkt == rbtP_HugePacket) {
				rbtNet_close(c);

				break;
			}

			if(pkt == rbtP_QQ_TGW_Packet) {
				continue;
			}

			if (rbtNet_is_encode(c)) {
				rbtP_head_decode(pkt);
				rbtP_decode(pkt);
			}

			short int seq = rbtP_get_seq(pkt);
			if (seq == c->recv_seq_expect) {
				c->recv_seq_expect++;
				if (c->recv_seq_expect > MAX_PACKET_SEQ) {
					c->recv_seq_expect = 1;
				}
			} else {
				kLOG(r, 0, "[Error]Except Seq_num(%d), Actually Seq_num(%d)\n", c->recv_seq_expect, seq);
				rbtP_drop(pkt);
				continue;
			}

			int fun = rbtP_get_fun(pkt);

			if(!r->is_client) {
				// server
				if(fun == 0 || PKT_IS_FUN(fun) ) {
					if(r->rpc_process) {
						int rpc_error = r->rpc_process(r, c, PKT_FUN_VALUE(fun), pkt);

						rbtP_drop(pkt);

						if(rpc_error < 0) {
							kLOG(r, 0, "[LOG]RPC 返回-1 , ip:%s, port:%d\n", rbtNet_ip(c), rbtNet_port(c));
							return -1;
						}
					}
					continue;
				}

				int req_id = PKT_REQ_VALUE(fun);
				rbtRpc_apply(r, c, req_id, pkt);

				rbtP_drop(pkt);

				continue;
			} else {
				// client
				if(c == r->data) {
					int req_id = PKT_REQ_VALUE(fun);
					rbtRpc_apply(r, c, req_id, pkt);

					rbtP_drop(pkt);
					continue;
				}

				if(r->rpc_process) {
					int subfun = fun & 0xFF;
					int rpc_error = r->rpc_process(r, c, subfun, pkt);

					rbtP_drop(pkt);

					if(rpc_error < 0) {
						kLOG(r, 0, "[LOG]RPC 返回-1 \n");
						return -1;
					}
				}
				continue;
			}
		} // while get next packet
	} // read data

	if(e & EVENT_WRITE ) {
		if( write_data( c ) < 0) {
			kLOG(r, 0, "[Error]写数据出错, ip:%s, port:%d\n", rbtNet_ip(c), rbtNet_port(c));
			return -1;
		}
	}

	return 0;
}

void rbtNet_conn_free(struct Connection * c)
{
	// 将没有发送的包抛弃
	while(!rbtQ_empty(c->write_queue)) {
		struct PacketNode * pn = rbtQ_peek(c->write_queue);
		if(pn && pn->pkt) {
			rbtP_drop(cast(Packet *, pn->pkt));
		}
		rbtQ_pop(c->write_queue);
	}

	// Read Buffer
	if(c->read_buffer) {
		buffer_free(c->read_buffer);
		c->read_buffer = NULL;
	}

	// Write Queue
	if(c->write_queue) {
		rbtQ_free(c->write_queue);
		c->write_queue = NULL;
	}

	// 从 unauthed list 中删除
	list_del(&c->unauthed_list);

	// 未完成的 Rpc 抛弃
	rbtRpc_conn_broken(c->r, c);

	//RFREE(c->r, c);
	rbtNet_set_status(c, CONN_CLOSED);
	list_insert(&g_FreeConn, &c->list);
}

struct Connection * rbtNet_construct( rabbit * r, int fd )
{
	if(!g_ConnInit) {
		g_ConnInit = 1;
		list_init(&g_FreeConn);
		int i;
		for(i = 0; i < MAX_CONN_NR; ++i) {
			struct Connection * tmp = &g_Conn[i];
			list_init(&tmp->list);
			list_insert(&g_FreeConn, &tmp->list);
			tmp->status = CONN_CLOSED;
		}
	}
	struct Connection * c;
	if(list_empty(&g_FreeConn)) {
		c = RMALLOC(r, struct Connection, 1);
		r->obj++;
		kLOG(r, 0, "[Warning]超出预分配的Connection数量！\n");
		c->status = CONN_CLOSED;

	} else {
		struct list_head * p = list_first_entry(&g_FreeConn);
		list_del(p);
		c = list_entry(p, struct Connection, list);
	}

	if(unlikely(c->status != CONN_CLOSED)) {
		kLOG(r, 0, "[Error] 新分配的Connection，Status(%d)不是CLOSED！\n", c->status);
	}

	c->r = r;

	list_init(&c->list);

	c->fd = fd;
	c->time = time(NULL);

	c->status = CONN_ESTABLISHED;

	c->authed = r->auth;
	list_init(&c->unauthed_list);

	c->read_buffer = buffer_init(r);

	c->write_queue = rbtQ_init(r, sizeof(struct PacketNode), 32);

	// clear eXtension
	c->x.p1 = c->x.p2 = 0;
	c->x.a1 = c->x.a2 = 0;

	// rpc
	list_init(&c->rpc_param);

	// statistic
	c->recv_nmem = 0;
	c->recv_npkt = 0;
	c->sent_nmem = 0;
	c->sent_npkt = 0;

	// ip & port
	memset(c->ip, 0, sizeof(c->ip));
	c->port = 0;

	c->sent_seq= 0;
	c->recv_seq_expect = 1;

	c->is_encode = 0;
	c->active_close = 0;

	return c;
}

int rbtNet_is_authed(Connection * c)
{
	return c->authed;
}

int rbtNet_set_authed(Connection * c, int auth)
{
	c->authed = auth;
	if(c->authed) {
		list_del(&c->unauthed_list);
	}

	return c->authed;
}

int rbtNet_send( rabbit * r, struct Connection * c, const Packet * pkt )
{
	if(rbtNet_status(c) != CONN_ESTABLISHED) {
		kLOG(r, 0, "[Error]给未确定的连接发送数据！(%d)\n", rbtNet_status(c));
		return 0;
	}
	if(!c->write_queue) {
		kLOG(r, 0, "[Error]给未确定的连接发送数据！write_queue不存在！(%d)\n", rbtNet_status(c));
		return 0;
	}

	rbtP_grab(cast(Packet *, pkt));

	struct PacketNode * pn = cast(struct PacketNode *, rbtQ_push(c->write_queue));
	pn->left = rbtP_size(pkt);
	pn->pkt = pkt;

	pn->mcurr = cast(struct MBlock *, pkt);
	char * p = cast(char *, pkt);
	pn->pcurr = p + sizeof(Packet);
	pn->pend = cast(char *, mblock_next(pn->mcurr));

	c->sent_npkt++;
	c->sent_seq++;
	if (c->sent_seq > MAX_PACKET_SEQ) {
		c->sent_seq = 1;
	}
	pn->pkt_seq = c->sent_seq;

	return write_data(c);
}

int rbtD_conn( Connection * c )
{
	return 0;
}

int rbtD_conn_mem(Connection * c)
{
	return sizeof(Connection);// + rbtD_buffer_mem(c->r, c->read_buffer) + rbtD_queue_mem(c->write_queue);
}

