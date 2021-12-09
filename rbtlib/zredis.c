#include "zredis.h"

#include "db.h"
#include "net_manager.h"
#include "connection.h"

typedef struct cmdlex {
	int keylen;			// key 不加头尾长度(头尾指redis格式的头尾)
	int valuelen;			// value 不加头尾长度
	char * key;			// 指向key起始位置
	char * value;			// 指向value起始位置
	char * p;			// 指向key-value串内容
}cmdlex;


int rbtR_conn_redis(rabbit * r, int ip, int port) {
	Connection * c = rbtNet_connect_try_once(r, ip, port);
	if (c) {
		rbtNet_set_authed(c, 1);
		r->redis = c;

		rLOG(r, "[log] 已连接上redis服务器。。。");
		kLOG(r, 0, "[log] 已连接上redis服务器。。。");

		return 0;
	}

	kLOG(r, 0, "[log] 连接redis服务器出错。。。");
	r->redis = NULL;

	return -1;

}

/* [内部接口]:实际发送所有命令的函数 */
static int __redis_send_cmd(int fd, const char * cmd, int len/* 指示此条命令字节数 */)
{
	int ret;
	while ( (ret = write(fd, (void *)cmd, len)) )
	{
		if (-1 == ret)
		{
			if (errno == EINTR)
				continue;
			fprintf(stderr, "[Error] write() error in __redis_send_cmd() \n");
			break;
		}
		else if (ret < len)
		{
			cmd += ret;
			len -= ret;
			continue;	
		}
		else if (ret == len)
			return 0;
	}

	return -1;
}

static int __get_key_start(cmdlex * lex)
{
	char * p = lex->p;
	while (*p == ' ' || *p == '\t')
	{
		p++;
	}
	if (*p == '\0')
	{
		fprintf(stderr, "[Warn] No key No value.\n");
		return -1;
	}

	lex->key = p;
	lex->p = p;

	return 0;
}
static int __get_key_len(cmdlex * lex)
{
	char * p = lex->p;
	while (*p != ' ' && *p != '\t' && *p != '\0')
	{
		p++;
		lex->keylen++;
	}
	if (*p == '\0')
	{
		fprintf(stderr, "[Warn] No value.\n");
		return -1;
	}
	*p = '\0';
	++p;
	if (lex->keylen > KEY_MAX)
	{
		fprintf(stderr, "[Error] Too big key.\n");
		return -1;
	}

	lex->p = p;
	return 0;
}

static int __get_value_start(cmdlex * lex)
{
	char * p = lex->p;
	while (*p == ' ' || *p == '\t')
	{
		p++;
	}
	if (*p == '\0')
	{
		fprintf(stderr, "[Warn] No value.\n");
		return 0;
	}

	lex->p = p;
	lex->value = p;

	return 0;
}
/* [内部接口]: */
static int __get_value_len(cmdlex * lex)
{
	char * p = lex->p;
	while (*p != '\0')
	{
		lex->valuelen ++;
		p++;
	}
	if (lex->valuelen > VALUE_MAX)
	{
		fprintf(stderr, "[Error] Too big value.\n");
		return -1;
	}
	
	lex->p = p;

	return 0;
}

// [内部接口]：往cmd中添加完整的key
static int __fill_with_key(cmdlex * lex, char * cmd)
{
	int khtlen = 0;

	khtlen = snprintf(cmd + LPUSH_OFFSET, KEY_MAX, "$%d\r\n%s\r\n", lex->keylen, lex->key);

	return khtlen;
}
// [内部接口]：往cmd中添加完整的value(附加了时间戳)
static int __fill_with_value(cmdlex * lex, char * cmd, int keyoffset)
{
	int vhtlen = 0;

	int timelen = 0;
	time_t rawtime;
	struct tm * timeinfo;
	char timebuf[TIME_MAX];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	memset(timebuf, 0, TIME_MAX);
	timelen = snprintf(timebuf, TIME_MAX, "%s", asctime(timeinfo));
	timebuf[timelen - 1] = '\0';		// 消除最后的 '\n'
	lex->valuelen = lex->valuelen + timelen -1;

	vhtlen = snprintf(cmd + LPUSH_OFFSET + keyoffset, VALUE_MAX, "$%d\r\n[%s]%s\r\n", lex->valuelen + 2, timebuf, lex->value);

	return vhtlen;
}

// [内部接口]
static int __parse_init(cmdlex * lex, char * keyvalue, char * fmt, va_list va)
{
	if ( fmt == NULL )
	{
		fprintf(stderr, "[Error] No fmt.\n");
		return -1;
	}

	if ( vsnprintf(keyvalue, CMD_MAX - LPUSH_OFFSET, fmt, va) < 0 )
	{
		return -1;
	}

	lex->p = keyvalue;
	lex->keylen = 0;
	lex->valuelen = 0;
	lex->key = NULL;
	lex->value = NULL;

	return 0;
}

// [内部接口]
static int __cmd_init(char * cmd, int op)
{
	switch (op)
	{
		case LPUSH_CMD:
			snprintf(cmd, LPUSH_OFFSET + 1, "*3\r\n$5\r\nlpush%s", "\r\n");
			return 0;
		default:
			return -1;
	}

	return -1;
}

int rbtR_cmd_lpush_single(int fd, char * fmt, ...)
{
	char cmd[CMD_MAX];			/* total cmd's buf */
	char keyvalue[CMD_MAX - LPUSH_OFFSET];	/* key + value's buf */
	cmdlex lex;
	int khtlen = 0, vhtlen = 0;

	memset(cmd, 0, sizeof(cmd));
	memset(keyvalue, 0, sizeof(keyvalue));

	va_list va;
	va_start(va, fmt);
	if ( __parse_init(&lex, keyvalue, fmt, va) == -1 )
	{
		fprintf(stderr, "[Error]: Parse Error.");
		return -1;
	}
	va_end(va);

	if ( __cmd_init(cmd, LPUSH_CMD) == -1 )
	{
		fprintf(stderr, "[Error]: Init Cmd Error.");
		return -1;
	}

	if ( *(lex.p) != '\0' )
	{
		if ( __get_key_start(&lex) < 0   || 
		     __get_key_len(&lex)   < 0   ||
	             __get_value_start(&lex) < 0 ||
		     __get_value_len(&lex) < 0	) 
		{
			return -1;
		}

		khtlen = __fill_with_key(&lex, cmd);
		vhtlen = __fill_with_value(&lex, cmd, khtlen);
	}
	else 
	{
		fprintf(stderr, "[Warn] Fmt Is NULL. No key No value.\n");
		return -1;
	}

	return __redis_send_cmd(fd, cmd, LPUSH_OFFSET + khtlen + vhtlen);
}
