#include "time_process_list.h"
#include "rabbit.h"
#include "mem.h"
#include "gc.h"
#include "table.h"

#define NODE_INIT_NUM	32

#define LINK_NULL	-1

#define ntop(n, list) cast(int, n - list->node)

struct node {
	unsigned int handler;

	int param;

	Table * argv;

	struct timeval t;

	int next;
	int prev;
};

struct tm_list {
	CommonHeader;

	int num;

	struct node * node;

	int size;

	int free;

	int first_ex;

	unsigned int handler;
};

static void traverse( GCObject * obj )
{
	tm_list * list = cast(tm_list *, obj);

	int ex = list->first_ex;
	while(ex != LINK_NULL) {
		struct node * n = &list->node[ex];
		rbtC_mark(n->argv);

		ex = n->next;
	}
}

static void release( GCObject * obj )
{
	tm_list * list = cast(tm_list*, obj);
	
	RFREEVECTOR(list->r, list->node, list->size);
	RFREE(list->r, list);
}

tm_list * tm_list_init( rabbit * r )
{
//	assert(0);

	struct tm_list * list = RMALLOC(r, struct tm_list, 1);

	rbtC_link(r, cast(GCObject *, list), TUSERDATA);

	list->gc_release = release;
	list->gc_traverse = traverse;

	list->node = RMALLOC(r, struct node, NODE_INIT_NUM);

	list->size = NODE_INIT_NUM;

	list->free = 0;

	list->handler = 0;

	int i;
	for(i = 0;i < NODE_INIT_NUM - 1; ++i) {
		struct node * n = &list->node[i];
		n->next = i + 1;
	}

	struct node * n = &list->node[NODE_INIT_NUM - 1];
	n->next = LINK_NULL;

	list->first_ex = LINK_NULL;

	list->num = 0;

	return list;
}

static void resize( tm_list * list )
{
	assert(list->free == LINK_NULL);

	list->node = RREALLOC(list->r, struct node, list->node, list->size, list->size + NODE_INIT_NUM);

	int i;
	for( i = 0;i < NODE_INIT_NUM - 1; ++i) {
		struct node * n = &list->node[list->size + i];
		n->next = list->size + i + 1;
	}

	struct node * n = &list->node[list->size + NODE_INIT_NUM - 1];

	n->next = LINK_NULL;

	list->free = list->size;

	list->size += NODE_INIT_NUM;
}

static int get_free_node( tm_list * list )
{
	if(list->free != LINK_NULL) {
		int pos = list->free;
		struct node * n = &list->node[pos];
		list->free = n->next;

		return pos;
	}

	resize(list);

	assert(list->free != LINK_NULL);

	return get_free_node(list);
}

static int insert( tm_list * list, int after, int pos )
{
	struct node * aft = &list->node[after];
	struct node * n = &list->node[pos];

	int pre = aft->prev;

	n->next = after;
	n->prev = aft->prev;
	aft->prev = pos;

	if(pre == LINK_NULL) {
		list->first_ex = pos;

		return 0;
	}

	struct node * prev = &list->node[pre];

	prev->next = pos;

	return 0;
}

int tm_list_insert_ex( tm_list * list, int param, Table * argv, struct timeval tm )
{
	int pos = get_free_node(list);

	struct node * free = &list->node[pos];

	free->handler = list->handler++;

	free->param = param;
	free->argv = argv;
	free->t = tm;

	list->num ++;

	if(list->first_ex == LINK_NULL) {
		list->first_ex = pos;
		free->next = free->prev = LINK_NULL;

		return 0;
	}

	int p = list->first_ex;

	struct node * n;

	while(p != LINK_NULL) {
		n = &list->node[p];

		if(timercmp(&n->t, &tm, >=)) {
			return insert(list, p, pos);
		}

		p = n->next;
	}

	n->next = pos;
	free->next = LINK_NULL;

	free->prev = cast(int, n - list->node);

	return 0;
}

int tm_list_next_ex( tm_list * list, struct timeval tm, Table ** argv )
{
	if(list->first_ex == LINK_NULL) {
		return -1;
	}

	int pos = list->first_ex;
	struct node * n = &list->node[pos];

	if(timercmp(&n->t, &tm, >)) {
		return -1;
	}

	list->num --;
	
	list->first_ex = n->next;

	if(list->first_ex != LINK_NULL) {
		struct node * next = &list->node[list->first_ex];
		next->prev = LINK_NULL;
	}

	n->next = list->free;
	list->free = pos;

	if(argv) {
		*argv = n->argv;
	}

	return n->param;
}

static void remove_at( tm_list * list, int pos )
{
	struct node * n = &list->node[pos];

	if(n->next != LINK_NULL) {
		struct node * next = &list->node[n->next];
		next->prev = n->prev;
	}

	if(n->prev != LINK_NULL) {
		struct node * prev = &list->node[n->prev];
		prev->next = n->next;
	} else {
		list->first_ex = n->next;
	}

	n->next = list->free;
	list->free = pos;

	list->num--;
}

int tm_list_remove( tm_list * list, int handler )
{
	int pos = list->first_ex;

	struct node * n;
	while(pos != LINK_NULL) {
		n = &list->node[pos];
		if(n->handler == handler) {
			remove_at(list, pos);
			break;
		}
		pos = n->next;
	}

	return 0;
}

