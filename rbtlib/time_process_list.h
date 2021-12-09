#ifndef time_process_list_h_
#define time_process_list_h_

#include "common.h"

typedef struct tm_list tm_list;

/*  	time process list
 *
 *	@param : gc -- 里面保存的值是不是需要gc, 如果需要gc, 要保证传进来的是 userdata * 指针
 *
 */
tm_list * tm_list_init( rabbit * r );

/*
 *	遍历时间到的事件,并将其返回
 */
int tm_list_next_ex( tm_list * list, struct timeval tm, Table ** argv );

/*
 *	注册时间事件
 */
int tm_list_insert_ex( tm_list * list, int param, Table * argv, struct timeval tm );

/*
 *	删除注册过的一个指针
 */
int tm_list_remove( tm_list * list, int handler );

#endif

