#include "net_manager.h"
#include "gc.h"
#include "mem.h"
#include "connection_struct.h"
#include "connection.h"
#include "rabbit.h"
#include "table.h"
#include "packet.h"
#include "remote_call.h"
#include "list.h"

struct NetManager {
	rabbit * r;

	// Listen fd
	int lfd;


	// Epoll
	int efd;
	struct epoll_event * events;
	int events_size;


	// 当前有多少连接
	int nconn;


	// 所有没有验证的连接，链在一起
	struct list_head unauthed_list;
};

int rbtNet_init( rabbit * r )
{
	struct NetManager * mgr = RMALLOC(r, struct NetManager, 1);
	
	mgr->r = r;

	mgr->nconn = 0;
	mgr->events = NULL;
	mgr->events_size = 0;

	list_init(&mgr->unauthed_list);

 	if(-1 == (mgr->efd = epoll_create(r->max_conns))) {
		perror("epoll create failed.");
		return -1;
	}

	r->net_mgr = mgr;
	r->obj++;

	rbtPacket_init(r);

	return 0;
}

int isetnonblock( int fd )
{
	int flag = fcntl(fd, F_GETFL,0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);

	int val = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		perror("Set Sock Opt Error");
		return -1;
	}

	return 0;
}


static int iepoll_add( struct NetManager * mgr, int fd, int e, void * data ) 
{
	struct epoll_event ee;
	memset(&ee, 0, sizeof(struct epoll_event));
	if(e & EVENT_READ) {
		ee.events |= EPOLLIN;
	}
	if(e & EVENT_WRITE) {
		ee.events |= EPOLLOUT;
	}
	ee.events |= EPOLLET;

	//ee.data.fd = fd;
	ee.data.ptr = data;

	if(epoll_ctl(mgr->efd, EPOLL_CTL_ADD, fd, &ee) < 0) {
		perror("epoll add failed.");
		return -1;
	}

	return 0;
}

// 新来一个链接
static struct Connection * iconn_add( struct NetManager * mgr, int fd, const char * ip, int port )
{
	rabbit * r = mgr->r;

	isetnonblock(fd);

	Connection * conn = rbtNet_construct(r, fd);
	if(!conn) {
		close(fd);
		return NULL;
	}
	list_insert_tail(&mgr->unauthed_list, &conn->unauthed_list);

	if(iepoll_add(mgr, fd, EVENT_READ | EVENT_WRITE, conn) < 0) {
		return NULL;
	}

	mgr->nconn++;

	rbtNet_set_status(conn, CONN_ESTABLISHED);

	if(ip) {
		int ip_len = min(15, strlen(ip));
		memcpy(conn->ip, ip, ip_len);
		conn->ip[ip_len] = 0;
		conn->port = port;
		kLOG(r, 0, "[LOG]新建Connection, ip:%s, port:%d, 总连接数：%d", ip, port, mgr->nconn);
	}

	return conn; 
}

//  一个链接断开了，彻底删除
static int iconn_del( struct NetManager * mgr, struct Connection * c )
{
	rabbit * r = mgr->r;

	if(c->status == CONN_CLOSED) {
		return 0;
	}

	c->status = CONN_CLOSED;

	if(!c->active_close && r->conn_broken) {
		r->conn_broken(r, c);
	}

	int fd = rbtNet_fd(c);

	// 从 epoll 里删除，本来 close(fd) 应该默认就从epoll里删除了，为了以防万一，通过 epoll_ctl 显式删除
	// 在 man epoll 里，也提到，如果一个Connection的关闭是由于另外的Connection引起的，那么需要显式的删除(epoll_ctl(del))，不然这个Connection有可能被
	// 之前发生的事件提示
	epoll_ctl(mgr->efd, EPOLL_CTL_DEL, fd, NULL);

	close(fd);

	mgr->nconn--;

	// 释放 connection 空间 XXX...
	rbtNet_conn_free(c);

	kLOG(r, 0, "网络连接断开：%d\n", fd);

	return 0;
}

static int listen_wake_up(struct NetManager * mgr)
{
	rabbit * r = mgr->r;

	struct sockaddr_in addr;
	int addr_len = sizeof(addr);

	int fd;
	while( 1 ) {
begin:
		fd = accept(mgr->lfd, (struct sockaddr*)&addr, (socklen_t*)&addr_len);
		if(fd < 0) {
			switch( errno ) {
				case EINTR:
					goto begin;

				default:
					break;
			}
			return 0;
		}
		struct sockaddr_in addr = { AF_INET, 0, { 0 } };
		socklen_t len = sizeof(addr);

		int err = 0;
		if((err = getpeername(fd, (struct sockaddr*)(&addr), &len))) {
			kLOG(r, 0, "[Error]网络新连接到来，getpeername失败！error:%d", err);
			close(fd);
			break;
		}

		const char * ip = inet_ntoa(addr.sin_addr);
		int port = ntohs(addr.sin_port);

		kLOG(r, 0, "[LOG]网络新连接 : %d, ip(%s), port(%d).\n", fd, ip, port);

		iconn_add(mgr, fd, ip, port);
	}

	return 0;
}

static int rbtNet_listen_raw( rabbit * r , int port , uint32_t s_addr){
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if(fd < 0) {
		perror("socket error");
		return -1;
	}

	isetnonblock(fd);

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(s_addr);

	if(bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
		perror("bind error");
		return -1;
	}

	kLOG(r, 0, "网络初始化，listen at : %d\n", port);

	if(listen(fd, 1024) < 0) {
		perror("listen error");
		return -1;
	}

	struct NetManager * mgr = r->net_mgr;

	iepoll_add(mgr, fd, EVENT_READ, NULL);

	mgr->nconn++;

	mgr->lfd = fd;

	return 0;
}

int rbtNet_listen( rabbit * r, int port )
{
	return rbtNet_listen_raw( r , port , INADDR_ANY );
}

int rbtNet_listen_loopback( rabbit * r, int port )
{
	return rbtNet_listen_raw(r , port , INADDR_LOOPBACK);
}


int rbtNet_poll( rabbit * r, time_t milsec )
{
	struct NetManager * mgr = r->net_mgr;

	if(mgr->nconn < 1) {
		return 0;
	}

	if(mgr->nconn > mgr->events_size) {
		mgr->events = RREALLOC(r, struct epoll_event, mgr->events, mgr->events_size, mgr->nconn + 1024);
		mgr->events_size = mgr->nconn + 1024;
	}

	int count;
	if((count = epoll_wait( mgr->efd, mgr->events, mgr->events_size, milsec )) == -1) {
		switch( errno ) {
			case EINTR:
				return 0;
			default:
				perror("epoll wait failed");
				break;
		}
		return -1;
	}

	for(; count; count--) {
		struct Connection * c = mgr->events[count-1].data.ptr;
		int event = mgr->events[count-1].events;

		int e = 0;
		if(event & EPOLLIN) {
			e |= EVENT_READ;
		}
		if(event & EPOLLOUT) {
			e |= EVENT_WRITE;
		}
		if(event & EPOLLHUP) {
			e |= EVENT_HUP;
		}
		if(event & EPOLLERR) {
			e |= EVENT_ERR;
		}

		if(!c) {	// Listen fd
			if(e & EVENT_READ) {
				listen_wake_up(mgr);
			}
			continue;
		}

		int process_error = rbtNet_process(c, e);

		if( process_error < 0 || (e & EVENT_HUP) || (e & EVENT_ERR) ) {
			kLOG(r, 0, "处理有误，或收到 EVENT_HUP || EVENT_ERR 断开连接, fd : %d\n", rbtNet_fd(c));
			iconn_del( mgr, c );
			continue;
		}

		if(rbtNet_status(c) == CONN_ZOMBIE && rbtNet_empty(c)) {
			kLOG(r, 0, "Zombie，断开连接, fd : %d\n", rbtNet_fd(c));
			iconn_del( mgr, c );
		}
	}

	// 没有验证的链接，10秒后关闭
	struct list_head * elem, * tmp;
	time_t tm = time(NULL);
	list_foreach_safe(elem, tmp, &mgr->unauthed_list) {
		Connection * c = list_entry(elem, Connection, unauthed_list);
		if(tm - c->time < 10) {
			break;
		}

		list_del(elem);

		if(!rbtNet_is_authed(c)) {
			kLOG(r, 0, "连接10秒内没有验证，断开连接, fd : %d!\n", rbtNet_fd(c));
			iconn_del(mgr, c);
		}
	}

	return 0;
}

struct Connection * rbtNet_connect_try_hardly( rabbit * r, int address, int port )
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = address;

	kLOG(r, 0, "尝试链接……\n");
	if(connect(fd, (struct sockaddr*)(&addr), sizeof(addr)) < 0) {
		kLOG(r, 0, "失败！(%d)\n", errno);
		close(fd);
		sleep(1);
		return rbtNet_connect_try_hardly(r, address, port);
	}

	struct in_addr in;
	in.s_addr = address;
	struct Connection * c = iconn_add(r->net_mgr, fd, inet_ntoa(in), ntohs(port));
	if(c) {
		rbtNet_set_authed(c, 1);
	}

	return c;
}

struct Connection * rbtNet_connect_try_once( rabbit * r, int address, int port )
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = address;

	kLOG(r, 0, "尝试连接……\n");
	if(connect(fd, (struct sockaddr*)(&addr), sizeof(addr)) < 0) {
		kLOG(r, 0, "失败！(%d)\n", errno);
		close(fd);
		return NULL;
	}

	struct in_addr in;
	in.s_addr = address;
	struct Connection * c = iconn_add(r->net_mgr, fd, inet_ntoa(in), ntohs(port));
	if(c) {
		rbtNet_set_authed(c, 1);
	}

	return c;
}

// 软关闭，设为 ZOMBIE 状态，当上面待发送的包发送完成后，再关闭
int rbtNet_close( struct Connection * c )
{
	if(!c) {
		return 0;
	}
	rabbit * r = c->r;
	struct NetManager * mgr = r->net_mgr;

	if(rbtNet_status(c) != CONN_ESTABLISHED) {
		return 0;
	}

	rbtNet_set_status(c, CONN_ZOMBIE);

	rbtNet_process(c, EVENT_WRITE);

	// 主动close，不进行回调
	c->active_close = 1;

	if(rbtNet_empty(c)) {
		kLOG(r, 0, "Close Zombie 主动将连接断开, fd : %d\n", rbtNet_fd(c));
		return iconn_del(mgr, c);
	}

	return 0;
}

int rbtNet_mgr_count(rabbit * r)
{
	if(r->net_mgr) {
		return 1;
	}
	return 0;
}

int rbtNet_mgr_mem(rabbit * r)
{
	if(r->net_mgr) {
		return sizeof(struct NetManager) + r->net_mgr->events_size * sizeof(struct epoll_event);
	}
	return 0;
}
