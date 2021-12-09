#ifndef pool_h_
#define pool_h_

#include "object.h"


/*
 *	初始化一个 Pool
 *
 *	@param r
 *	@param elem_size	-- 每个元素的大小
 *	@param init_size	-- 初始化的元素个数，缓存区只会增，不减
 */
struct Pool * rbtPool_init( rabbit * r, int elem_size, int init_size);

void rbtPool_destroy( struct Pool * pool );

/*
 *	为 Pool 里的元素设置 遍历 函数
 *
 *	@param pool
 *	@param f	-- Pool 里元素的 遍历 函数
 */
void rbtPool_traverse( Pool * pool, void(*f)(rabbit * r, void * p) );


/*
 *	寻找一个空位置，返回位置 id
 *
 *	@param pool
 */
int rbtPool_push( struct Pool * pool );


/*
 *	通过 id 取得位置的地址
 *
 *	@param pool
 *	@param id
 */
void * rbtPool_at( struct Pool * pool, int id );


/*
 *	释放 id 所标识的位置
 *
 *	@param pool
 *	@param id
 */
int rbtPool_free( struct Pool * pool, int id );

/*
 *	获得缓冲池内元素的个数
 *
 *	@param pool
 */
int rbtPool_count( struct Pool * pool );

int rbtPool_mem( struct Pool * pool );

int rbtPool_debug_count();
int rbtPool_debug_mem();

#endif

