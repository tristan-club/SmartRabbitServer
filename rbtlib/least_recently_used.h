#ifndef least_recently_used_h_
#define least_recently_used_h_

#include "object.h"


/*
 *	初始化一个 LRU 缓存池
 *
 *	@param r
 *	@param elem_size	-- 每一个储存元素的大小
 *	@param size		-- 缓存池的大小
 */
struct Lru * rbtLru_init( rabbit * r, size_t elem_size, size_t size );


/*
 *	设置 LRU 元素 Traverse 函数
 *
 *	@param list
 *	@param f
 */
void rbtLru_traverse( struct Lru * list, void (* f)( rabbit * r, void * p ) );

/*
 *	由 lru 算法，获得一个最低优先级的位置
 *
 *	@param list
 */
void * rbtLru_push( struct Lru * list );


/*
 *	访问一个地址，会让这个刚访问的地址优先级+1
 *
 *	@param list
 *	@param p
 */
void rbtLru_visit( struct Lru * list, void * p );

#endif

