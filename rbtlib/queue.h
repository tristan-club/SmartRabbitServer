#ifndef queue_h_
#define queue_h_

#include "object.h"

/*
 *	初始化一个 Queue
 *
 *	@param r
 *	@param elem_size	-- 每个元素的大小
 *	@param nelem		-- 初始化元素个数
 */
Queue * rbtQ_init( rabbit * r, size_t elem_size, size_t nelem );


/*
 *	释放一个 Queue
 *
 *	@param q
 */
void rbtQ_free( struct Queue * q );


/*
 *	新增一个元素，返回地址
 *
 *	@param q
 */
void * rbtQ_push( Queue * q );

/*
 *	取的第一个元素地址
 *
 *	@param q
 */
void * rbtQ_peek( Queue * q );

/*
 *	删除第一个元素
 *
 *	@param q
 */
void rbtQ_pop( Queue * q );

/*
 *	判断Queue是否为空
 *
 *	@param q
 */
int rbtQ_empty( Queue * q );

/*
 *	清空Queue
 *
 *	@param q
 */
int rbtQ_clear( Queue * q );

int rbtQ_count( Queue * q );


/*
 *	打印Queue的信息
 *
 *	@param q
 */
int rbtD_queue( const Queue * q );

int rbtD_queue_mem(const Queue * q);

int rbtQ_debug_count();
int rbtQ_debug_mem();

#endif

