#ifndef common_h_
#define common_h_

#include <stddef.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <string.h>
#include <sys/resource.h>

#include <math.h>

#include <dlfcn.h>

#include <dirent.h>

#include <sys/stat.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <time.h>

#include <linux/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <netdb.h>

#include <assert.h>

#include <endian.h>	// for byte order

#include <mysql.h>
#include <syslog.h> 

#include "compiler.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define IS_BIG_ENDIAN	0
#	define IS_LITTLE_ENDIAN	1
#else
#	define IS_BIG_ENDIAN	1
#	define IS_LITTLE_ENDIAN	0
#endif

#define revert_4( i ) ( (((i) << 24) & 0xff000000) | ( ((i) << 8) & 0xff0000 ) | ( ((i) >> 8) & 0xff00 ) | ( ((i) >> 24) & 0xff ) )
#define revert_2( i ) ( ( ((i) << 8) & 0xff00 ) | ( ((i) >> 8) & 0xff ) )

typedef enum { 
	// simple data type -- don't need gc
		TNIL,
		TNUMBER,
		TBOOL,
		TFLOAT,
		TDATE,
		TPOINTER,

	// not gc
		TPACKET,
		TCONNECTION,
		TBUFFER,
		TQUEUE,
		TPOOL,
		
		// 脚本函数，始终在内存，不需要回收
		TPROTO,


	// gc object
	TGC,
		// data structure
		TSTRING,
		TLRU,
		TTABLE,

		// file
		TSTREAM,

		// network
		TNETMANAGER,

		// GCObject
		TUSERDATA,

		// script
		TSCRIPT,
		TSCRIPTX,
		TCLOSURE,
		TCONTEXT,

	TTYPE_END,

} E_TYPE;

#define is_collectable(o) (ttype(o) > TGC && ttype(o) < TTYPE_END)

extern const char * E_TYPE_STR[];

#include "declare.h"

rabbit * rabbit_init( void );

#define cast(t,o) ((t)(o))

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef abs
#define abs(x) ((x) >= 0 ? (x) : (-1 * (x) ))
#endif

#ifndef max
#define max(x,y) ((x) >= (y) ? (x) : (y))
#endif

#ifndef min
#define min(x,y) ((x) > (y) ? (y) : (x))
#endif

#ifndef sgn
#define sgn(x)	((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))
#endif

#ifndef fequal
#define fequal(x, y) (!!((x) == (y)))
#endif

#ifndef clamp
#define clamp(x, a, b)	\
	({	\
	 	double _____x = x;	\
	 	double _____a = a;	\
	 	double _____b = b;	\
	 	double _____c = max(_____a, min(_____x, _____b));	\
	 	_____c;	\
	 })
#endif

#define POINTER_SIZE sizeof(void*)
#define INT_SIZE sizeof(int)
#define DOUBLE_SIZE sizeof(double)
#define CHAR_SIZE sizeof(char)

#ifndef offsetof
#define offsetof(type, member) (size_t)(&((type *)0)->member)
#endif

#ifndef container_of
#define container_of(p, type, member)	({	\
		typeof(((type *)0)->member) * _mptr = (p);	\
		(type*)((char*)_mptr - offsetof(type, member));})
#endif


#include "fill_stream.h"

#define LOG(fmt, ...)   fprintf(stderr, "[LOG]<%s>%s(%d):"fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#ifdef NPRINT
#define fprintf(file, ...)
#endif

#ifndef NDEBUG
#define kLOG(r, pid, fmt, ...) do{	\
	if((r) && ((rabbit *)(r))->console) {	\
		fprintf(stderr, "[LOG:%d]:"fmt"\n", pid, ##__VA_ARGS__);	\
	}	\
	syslog(LOG_LOCAL0 | LOG_DEBUG, "[pid:%d]"fmt, pid, ##__VA_ARGS__);	\
}while(0)
#define kLOG_S(r, pid, fmt, ...) do{	\
	if((r) && ((rabbit *)(r))->console) {	\
		fprintf(stderr, "[LOG:%d]:"fmt"\n", pid, ##__VA_ARGS__);	\
	}	\
	syslog(LOG_LOCAL1 | LOG_DEBUG, fmt, ##__VA_ARGS__);	\
}while(0)
#else
#define kLOG(r, pid, fmt, ...)
#define kLOG_S(r, pid, fmt, ...)
#endif

extern char * program_name;

#define used(x)	((void)(x))

/* Time */
typedef unsigned long long msec_t;

#endif

