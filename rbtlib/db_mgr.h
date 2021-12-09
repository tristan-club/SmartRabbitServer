#ifndef _DB_MGR_H_
#define _DB_MGR_H_

#include "connection_struct.h"
#include "string.h"

#ifdef MULTIDB

#define MAX_DB_LIMIT 32

typedef struct DB {
	struct TString * name;
	struct Connection * c;
} DB;

/**
 *	向表中(和数组)中 添加/移除 一个DB(暂用Connection代替)
 *
 */
int rbtDM_new(rabbit * r, const TString * name, Connection * c);

/**
 *	@ret : 返回剩余 DB 个数
 *
 *	++ db 是在重连之后才删除的 , 由于 DB 中的信息在重连时需要使用
 */
int rbtDM_delete(rabbit * r, struct DB * db);
// int rbtDM_delete_name(rabbit * r, TString * name);
// int rbtDM_delete_conn(rabbit * r, struct Connection * c);

/**
 *	找到该 Connection 所属的 DB
 *
 */
struct DB * rbtDM_find_db(rabbit * r, Connection * c);

/**
 *	获取 DB 个数
 *
 */
int rbtDM_count(rabbit * r);


/**
 *	切换 DB
 *
 */
void rbtDM_switch(rabbit * r);

/**
 *	dump 出所有 DB in `db_array'
 *
 */
void rbtDM_dump(rabbit * r);

#endif

#endif /* _DB_MGR_H_ */
