#include "db_mgr.h"
#include "common.h"
#include "rabbit.h"
#include "connection.h"
#include "table.h"
#include "gc.h"

#ifdef MULTIDB

static struct DB db_array[MAX_DB_LIMIT];
static int cur_pos = -1;			// 当前DB的 pos, for `switch'
static int last_free_pos = 0;		// 有效 DB 的个数 , for `new' / `delete'


// TEST <fprintf>
#define SWITCHDB(r)	\
	do {	\
		cur_pos = (cur_pos+1) % last_free_pos;	\
		r->data = db_array[cur_pos].c;	\
	} while(0)

	// TEST in `SWITCHDB'
	// fprintf(stderr, "\n++ After SWITCH : cur_pos=%d, db_array[cur_pos].c=%p(name=%s), db-count=%d, r-data=%p \n", cur_pos, db_array[cur_pos].c, rbtS_gets(db_array[cur_pos].name), last_free_pos, r->data);

struct DB * rbtDM_find_db(rabbit * r, Connection * c) {
	int i;
	for(i=0; i < last_free_pos; i++) {
		if ( db_array[i].c == c ) {
			return &db_array[i];
		}
	}

	return NULL;
}

static void array_adjust(int pos) {
	int i = pos;
	while ( i < last_free_pos ) {
		db_array[i] = db_array[i+1];
		++i;
	}
}

static struct DB * array_add(rabbit * r, TString * name, Connection * c) {
	if ( last_free_pos + 1 > MAX_DB_LIMIT ) {
		return NULL;
	}

	struct DB * db = &db_array[last_free_pos];

	db_array[last_free_pos].c = c;
	db_array[last_free_pos].name = name;
	++last_free_pos;

	return db;
}

static void array_rm(rabbit * r, struct DB * db) {
	array_adjust(db - db_array);

	db_array[--last_free_pos].name = NULL;
	db_array[last_free_pos].c = NULL;

}

int rbtDM_new(rabbit * r, const TString * name, Connection * c) {
	struct DB * db = array_add(r, name, c);

	// 向 r->data_pool 中添加
	if ( !db ) {
		return -1;
	}

	setpvalue(rbtH_settstr(r, r->data_pool, name), db);
	rbtC_stable(cast(GCObject *, name));

	// 初始化 r->data
	if ( !r->data ) {
		r->data = c;
		cur_pos = 0;
	}

	return 0;
}

int rbtDM_delete(rabbit * r, struct DB * db) {
	rbtC_stable_cancel(cast(GCObject *, db->name));

	TValue * tv = rbtH_gettstr(r, r->data_pool, db->name);
	setnilvalue(tv);

	array_rm(r, db);

	// 最后一个必是 r->data
	if ( last_free_pos > 0 ) {
		SWITCHDB(r);		// 防止 r->data 为当前删除的 DB
	} else {
		r->data = NULL;
	}

	return last_free_pos;
}

inline void rbtDM_switch(rabbit * r) {
	SWITCHDB(r);
}

int rbtDM_count(rabbit * r) {
	used(r);
	return last_free_pos;
}


void rbtDM_dump(rabbit * r) {
	int i;
	for (i=0; i<last_free_pos; ++i) {
		if ( db_array[i].name && db_array[i].c ) {
			fprintf(stderr, "++++ [DUMP] index=%d , addr=%p, name=%s, c(addr)=%p , fd=%d \n\n", i, &db_array[i], rbtS_gets(db_array[i].name), db_array[i].c, db_array[i].c->fd);
		} else {
			fprintf(stderr, "[ERROR] db_array ERROR occurs(index = %d)...\n\n", i);
			break;
		}
	}
}

#endif
