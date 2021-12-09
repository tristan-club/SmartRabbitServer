#ifndef list_h_
#define list_h_

#ifndef ONLY_LIST
#include "common.h"
#else
#define offsetof(type, member) (size_t)(&((type *)0)->member)
#define container_of(p, type, member)	({	\
		typeof(((type *)0)->member) * _mptr = (p);	\
		(type*)((char*)_mptr - offsetof(type, member));})
#endif

// 双链表
struct list_head {
	struct list_head * prev, * next;
};

#define list_empty(head) ((head)->prev == (head))

#define LIST_HEAD(decl)	struct list_head decl = { .prev = &decl, .next = &decl }

static inline void list_del(struct list_head * elem);

static inline void _insert_at(struct list_head * prev, struct list_head * next, struct list_head * elem)
{
	prev->next = elem;
	elem->prev = prev;
	elem->next = next;
	next->prev = elem;
}

static inline void list_init(struct list_head * head) 
{
	head->prev = head;
	head->next = head;
}

static inline void list_insert(struct list_head * head, struct list_head * elem)
{
/*	if(!list_empty(elem)) {
		fprintf(stderr, "[Error] list_insert, Elem %p is not empty!\n", elem);
		syslog(LOG_LOCAL0 | LOG_DEBUG, "[Error] list_insert, Elem %p is not empty!\n", elem);
		list_del(elem);
	}*/
	_insert_at(head, head->next, elem);
}

static inline void list_insert_tail(struct list_head * head, struct list_head * elem)
{
	_insert_at(head->prev, head, elem);
}

static inline void list_del(struct list_head * elem)
{
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	list_init(elem);
}

static inline void list_copy(struct list_head * to, struct list_head * from)	
{
	if(list_empty(from)) {
		list_init(to);
	} else {
		to->next = from->next;
		from->next->prev = to;
		to->prev = from->prev;
		from->prev->next = to;
		list_init(from);
	}
}

#define list_first_entry(head)	((head)->next)

#define list_foreach(it, head)	for(it = (head)->next; it != (head); it = it->next)
#define list_foreach_reverse(it, head)	for(it = (head)->prev; it != (head); it = it->prev)

#define list_foreach_safe(it, tmp, head) for(it = (head)->next; (tmp = it->next) && (it != (head)); it = tmp)
#define list_foreach_reverse_safe(it, tmp, head)	for(it = (head)->prev; (tmp = it->prev) && (it != (head)); it = tmp)

#define list_entry(ptr, type, member)	\
	container_of(ptr, type, member)

#endif

