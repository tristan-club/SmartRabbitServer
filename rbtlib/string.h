#ifndef string_h_
#define string_h_

#include "object.h"
#include "io.h"
#include "string_struct.h"


/*
 *	获取一个 TString
 *
 *	@param r
 *	@param str	-- 字符串
 */
const TString * rbtS_new( rabbit * r, const char * str );

/*
 *	获得 C String
 *
 *	@param ts
 */
//inline const char * rbtS_gets( const TString * ts );
#define rbtS_gets(ts)	(gets(ts))


/*
 *	获得 TString 的 长度
 *
 *	@param ts
 */
//inline int rbtS_len( const TString * ts );
#define rbtS_len(ts)	((ts)->len)

/*
 *	获得 TString 的 hash
 *
 *	@param ts
 */
//inline int rbtS_hash( const TString * ts );
#define rbtS_hash(ts)	((ts)->hash)


/*
 *	从一个字符串中获取一个 TString
 *
 *	@param r
 *	@param str	-- 字符串
 *	@param len	-- 要取的长度
 */
const TString * rbtS_init_len( rabbit * r, const char * str, int len );

/*
 *	从一个 io 里读取字符串
 *
 */
const TString * rbtS_init_io( rabbit * r, struct i_io * io, int len );


/*
 *	连接2个TString 为 一个新的 TString
 *
 *	@param r
 *	@param ts1
 *	@param ts2
 */
const TString * rbtS_concatenate( rabbit * r, const TString * ts1, const TString * ts2 );

/*
 *	将TString中的指定字符串替换为所需字符串	
 *
 *	@param r
 *	@param string
 *	@parma substr
 *	@param replacement
 */
const TString * rbtS_replace( rabbit *r, const TString *string, const TString *substr, const TString *replacement);

/*
 *	将1个TString中的字符全部转化为小写字符
 *
 *	@param r
 *	@param ts1
 */
const TString *rbtS_lowercase( rabbit * r, const TString * ts1 );

/*
 *	将一个字符串转化为一个Double类型的数字
 *
 *	@param ts
 */
double rbtS_tofnum( const TString * ts );


/*
 *	打印出TString 的信息
 *
 *	@param r
 *	@param ts
 */
int rbtD_string( const TString * ts );

int rbtS_debug_count();
int rbtS_debug_mem();

/*
 *	For VM
 */
inline TString * get_free_str(rabbit * r, int len);
inline void set_free_str(rabbit * r, TString * ts);
inline void link_ts(rabbit * r, TString * ts);

#endif
