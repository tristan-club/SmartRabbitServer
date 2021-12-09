#ifndef gc_h_
#define gc_h_

#include "object.h"

/*
 *	GC 状态
 *
 *
 */
enum {
	GCS_PAUSE,
	GCS_MARK,
	GCS_SWEEP,
	GCS_SWEEPSTRING
};

/*
 *	将一个GCObject 加入到垃圾回收系统里
 *
 *	@param r
 *	@param o
 *	@param tt	-- GCObject 的类型
 */
inline void rbtC_link( rabbit * r, GCObject * o , E_TYPE tt );

/*
 *	执行垃圾回收操作
 *
 *	@param r
 *	@param count	-- 要执行多少步
 */
void rbtC_step( rabbit * r, int count );

/*
 *	自动GC --
 *		1. 内存在100M内不进行GC
 *			
 *		2. 内存超出100M后，如果内存比上次比没有增长，则执行r->obj * 1%次GC
 *
 *		3. 内存增长超过50%，进行full_gc
 *
 *		4. 内存每增长x%，进行r->obj * x%次GC
 *
 *
 */
void rbtC_auto_gc(rabbit * r);

/*
 *	标记一个GCObject，实际上是将其存入 *垃圾回收待标记队列* 里
 *
 *	@param o
 */
inline void rbtC_mark( void * o );

/*
 *	设置非回收
 *
 *	@param o
 */
void rbtC_stable( GCObject * o );

void rbtC_stable_cancel( GCObject * o );

/*
 *	Full GC
 *
 */
void rbtC_full_gc( rabbit * r );

#endif

