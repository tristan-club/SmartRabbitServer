#include "script_struct.h"


/*
 *	新建一个Proto
 *
 *	@param r
 */
Proto * rbtP_proto( rabbit * r );

/*
 *	打印一个Proto信息
 *
 *	@param p
 */
void rbtD_proto( Proto * p );


/*
 *	将一个Proto信息打印到文件里
 *
 *	@param p
 *	@param fname
 */
void rbtD_proto_to_file( Proto * p, const char * fname );

int rbtD_proto_mem(Proto * p);

#define getni(P,n) ( &(P)->i[n] )

