#ifndef table_h_
#define table_h_

#include "object.h"

/*
 *	构建一个Table, Table 里有2个储存区域，一个是数组，一个是key-value的hash表，由数字进行索引的尽量存在数组里，取时会快一些
 *
 *	@param r
 *	@param vsize	-- 数组的初始化大小
 *	@param tsize	-- Hash 表的初始化大小
 */
Table * rbtH_init( rabbit * r, size_t vsize, size_t tsize );

Table * rbtH_init_no_link( rabbit * r, size_t vsize, size_t tsize );

/*
 *	设置 Table 为 弱引用	-- Table 里储存的不算做引用，默认为强引用，一般如果Table里存的是非GCObject，则设为弱引用可以在GC Mark是不参与遍历，减少操作
 *
 *	@param t
 */
void rbtH_weak( Table * t );

/*
 *	删除 Table 里的 key
 *
 */
#define rbtH_rmstr(r,t,key) {	\
	const TValue * i_v = rbtH_getstr(r,t,key);	\
	if(!ttisnil(i_v)) {	\
		setnilvalue(cast(TValue*,i_v));	\
	}	\
}

#define rbtH_rmnum(r,t,key) {	\
	const TValue * i_v = rbtH_getnum(r,t,key);	\
	if(!ttisnil(i_v)) {	\
		setnilvalue(cast(TValue*,i_v));	\
	}	\
}

/*
 *	通过 key 取得 Table 里的 value
 *
 *	@param r
 *	@param t
 *	@param key
 */
inline const TValue * rbtH_getstr( rabbit * r, Table * t, const char * key );
inline const TValue * rbtH_gettstr( rabbit * r, Table * t, const TString * key );
inline const TValue * rbtH_getnum( rabbit * r, Table * t, int key );
inline const TValue * rbtH_get( rabbit * r, Table * t, const TValue * key );


/*
 *	在 Table 里取得一个 key 的位置
 *
 *	@param r
 *	@param t
 *	@param key
 */
inline TValue * rbtH_setstr( rabbit * r, Table * t, const char * key );
inline TValue * rbtH_settstr( rabbit * r, Table * t, const TString * key );
inline TValue * rbtH_setnum( rabbit * r, Table * t, int key );
inline TValue * rbtH_set( rabbit * r, Table * t, const TValue * key );


/*
 *	在 Table 里，以下一个数字做 key 取得位置
 *
 *	@param r
 *	@param t
 */
inline TValue * rbtH_setnextnum( rabbit * r, Table * t );

/*
 *	取得 Table 里元素的数量
 *
 *	@param r
 *	@param t
 */
int rbtH_count( rabbit * r, Table * t );
int rbtH_countnum( rabbit * r, Table * t );
int rbtH_countstr( rabbit * r, Table * t );


/*
 *	遍历一个 Table
 *
 *	@param r
 *	@param t
 *	@param idx	-- 第一次传入-1，以后将rbtH_next 返回值传入，当遍历完全后，rbtH_next 返回-1
 *	@param k	-- 取得 key
 *	@param v	-- 取得 val
 */
int rbtH_next( rabbit * r, Table * t, int idx, TValue * k, TValue * v );


/*
 *	Table 是否为空
 *
 *	@param r
 *	@param t
 */
int rbtH_empty( rabbit * r, Table * t );


/*
 *	Table 清空
 *
 *
 */
void rbtH_clean( rabbit * r, Table * t );

/*
 *	Table -- 只给VM使用！
 *
 */
inline TValue * rbtHVM_newkey(rabbit * r, Table * t, const TValue * key);

/*
 *	打印一个 Table 的信息
 *
 *	@param r
 *	@param t
 */
int rbtD_table( rabbit * r, Table * t );
int rbtLog_table( rabbit * r, Table * t, int pid );

/*
 *	Table 所占内存
 *
 */
int rbtM_table( Table * t );

int rbtH_debug_count();
int rbtH_debug_mem();

#endif

