#ifndef table_struct_h_
#define table_struct_h_

#include "list.h"

struct Node {
	TValue key;
	TValue val;
	struct Node * next;
};

struct Table {
	CommonHeader;

	TValue * vector;
	size_t vector_size;

	struct Node * table;
	size_t table_size;

	int last_free_pos;

	int weak;     // 里面保存的东西不需要gc, 默认是需要gc的

	int state;	// XXX 在实现upvalue之前，简单实现标记这个Table是不是用户user info/user item，是哪个用户的
	int pid;

	struct list_head link;	// 所有的Table链在一起，用于debug统计信息
};

#define gnode(t,i) (&((t)->table[i]))
#define gkey(n) (&((n)->key))
#define gval(n) (&((n)->val))

#endif

