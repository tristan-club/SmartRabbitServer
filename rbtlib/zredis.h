#ifndef __ZREDIS_H_
#define __ZREDIS_H_

/* 插入一个key一个value的命令的格式 ："*3\r\n$5\r\nlpush\r\n$<keybytes>\r\n<key>\r\n$<valuebytes>\r\n<value>\r\n" */
#define CMD_MAX 		1024 * 100 - 1				/* 每条命令最大长度 */
#define KEY_MAX 		256 - 1					/* 每个key最大长度 */
#define VALUE_MAX 		(CMD_MAX - KEY_MAX - LPUSH_OFFSET) 	/* 每个value最大长度 */

#define KEYHEAD_MAX		6					/* key的头部的最大长度 $byte\r\n */
#define VALUEHEAD_MAX		6					/* key的尾部的最大长度 \r\n */
#define KEYTAIL_MAX		2					/* value的头部的最大长度 */
#define VALUETAIL_MAX		2					/* value的尾部的最大长度 */
#define TIME_MAX		30					/* 如果英文简写都精确的话，还可以减小该值 */

/* 各个命令偏移 */
#define LPUSH_OFFSET 		15 		/* 由于字符串前面是固定的，所以是固定的 15 */

/* 命令 */
#define LPUSH_CMD		101		/* list 的lpush命令，插入单个value */

#include "rabbit.h"
#include "connection_struct.h"

/*
 *
 * Redis命令 : lpush - 往list头中插入 一个 value(非阻塞操作)
 * 	@param fmt - 格式化字符串（此处的key - value分隔符为fmt串中第一个空格来决定的，包括后面的不定个数的参数中的空格）
 * 	@param ... - 传入的字符串至少有一个空格，以第一个空格来区分 key value
 * */
int rbtR_cmd_lpush_single(int fd, char * fmt, ...);

/*
 * 连接redis服务器
 *
 * */
int rbtR_conn_redis(rabbit * r, int ip, int port);

#define rLOG(r,fmt,...) do{ \
		if((r) && ((rabbit *)(r))->console){ \
			fprintf(stderr, "[LOG:DataServer] : "fmt"\n", ##__VA_ARGS__); \
		} \
		if((r)) { \
			rbtR_cmd_lpush_single( ( (Connection *)(((rabbit *)(r))->redis) )->fd, "[LOG:DataServer] "fmt, ##__VA_ARGS__ ); \
		} \
}while(0)

#endif /* __ZREDIS_H_ */
