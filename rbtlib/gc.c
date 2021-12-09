#include "gc.h"

#include "object.h"
#include "rabbit.h"
#include "queue.h"
#include "table.h"
#include "script.h"
#include "pool.h"
#include "net_manager.h"

#include "string_struct.h"

#include "mem.h"

#include "remote_call.h"
#include "script_misc.h"
#include "list.h"
#include "statistic.h"

#define GC_STABLE       0x1000

#define gmark(o) cast(GCObject*, o)->gch.mark

#define mark_stable(gc) gmark(o) = GC_STABLE

#define is_stable(gc) (gmark(o) == GC_STABLE)

#define mark_white(o) \
	        { if(gmark(o) != GC_STABLE) { cast(GCObject*, o)->gch.mark = o->gch.r->currentwhite; } }

static int g_nGC = 0;	// 统计每次gc，遍历了多少Object
static int g_nFree = 0;	// 每次gc，释放了多少Object
static int g_nMark = 0;	// 每次gc，Mark了多少Object

void rbtC_stable( GCObject * o )
{
	if(is_stable(o)) {
		return;
	}
	rabbit * r = o->gch.r;
	used(r);

	/*
	GCObject *prev = NULL, *curr;
	curr = r->gclist;
	while(curr) {
		if(curr == o) {
			if(prev) {
				prev->gch.gclist = curr->gch.gclist;
			} else {
				r->gclist = curr->gch.gclist;
			}
			return;
		}
		prev = curr;
		curr = curr->gch.gclist;
	}
	curr = r->gray;
	prev = NULL;
	while(curr) {
		if(curr == o) {
			if(prev) {
				prev->gch.gclist = curr->gch.gclist;
			} else {
				r->gclist = curr->gch.gclist;
			}
			return;
		}
		prev = curr;
		curr = curr->gch.gclist;
	}*/

	mark_stable(o);
}

void rbtC_stable_cancel( GCObject * o )
{
	rabbit * r = o->gch.r;
	gmark(o) = o->gch.r->currentwhite;
	/*
	if(!o->gch.gclist) {
		o->gch.gclist = r->gclist;
		r->gclist = o;
	}*/
	if(!list_empty(&o->gch.gclist)) {
		list_insert(&r->gclist, &o->gch.gclist);
	}
}

inline void rbtC_link( rabbit * r, GCObject * o , E_TYPE tt)
{
	/*
	o->gch.gclist = r->gclist;
	r->gclist = o;
	*/
	list_init(&o->gch.gclist);
	list_insert(&r->gclist, &o->gch.gclist);

	o->gch.mark = r->currentwhite;
	o->gch.tt = tt;

	o->gch.gc_traverse = NULL;
	o->gch.gc_release = NULL;

	o->gch.r = r;

	switch( tt ) {
		case TSTRING:
			o->gch.debug_name = "String";
			break;
		case TBUFFER:
			o->gch.debug_name = "Buffer";
			break;
		case TQUEUE:
			o->gch.debug_name = "Queue";
			break;
		case TLRU:
			o->gch.debug_name = "Lru";
			break;
		case TTABLE:
			o->gch.debug_name = "Table";
			break;
		case TPOOL:
			o->gch.debug_name = "Pool";
			break;
		case TSTREAM:
			o->gch.debug_name = "Stream";
			break;
		case TNETMANAGER:
			o->gch.debug_name = "Net Manager";
			break;
		case TCONNECTION:
			o->gch.debug_name = "Connection";
			break;
		case TUSERDATA:
			o->gch.debug_name = "UserData";
			break;
		case TSCRIPT:
			o->gch.debug_name = "Script";
			break;
		case TSCRIPTX:
			o->gch.debug_name = "ScriptX";
			break;
		case TPROTO:
			o->gch.debug_name = "Proto";
			break;
		case TCLOSURE:
			o->gch.debug_name = "Closure";
			break;
		case TCONTEXT:
			o->gch.debug_name = "Context";
			break;

		default:
			o->gch.debug_name = "Unknown";
			break;
	}

	r->obj++;
}

static void traverse_rabbit( rabbit * r )
{
	if(r->_G) {
		rbtC_mark(cast(GCObject *, r->_G));
	}

	rbtStat_rt_gc(r);

	rbtScript_traverse(r);
	rbtRpc_traverse(r, r->rpc);
}

static void rbtC_begin( rabbit * r )
{
//	r->gray = r->gclist;
//	r->gclist = NULL;
	list_copy(&r->gray, &r->gclist);
	list_init(&r->gclist);
//	list_copy(&r->gclist, &r->green);
//	list_init(&r->green);

	r->currentwhite = 1 - r->currentwhite;
	r->gc_status = GCS_MARK;

	g_nGC = 0;
	g_nMark = 0;
	g_nFree = 0;

	traverse_rabbit( r );
}

static void object_release( rabbit * r, GCObject * obj )
{
	r->obj--;

	if(obj->gch.gc_release) {
		obj->gch.gc_release(obj);
	}
}

static inline void sweep_step( rabbit * r, int count ) 
{
	GCObject * obj;
	/*
	while(count-- && r->gray) {
		obj = r->gray;
		r->gray = r->gray->gch.gclist;
		if(gmark(obj) == GC_STABLE) {
			// do nothing
			// 如果是stable，则不将其放在 gc链 上
		} else
		if(gmark(obj) == r->currentwhite) { // || gmark(obj) == GC_STABLE) {
			obj->gch.gclist = r->gclist;
			r->gclist = obj;
		} else {
			object_release( r, obj );
			g_nFree++;
		}

		g_nGC++;
	}
	*/

	struct list_head * p;
	while(count-- && !list_empty(&r->gray)) {
		p = list_first_entry(&r->gray);
		list_del(p);
		obj = cast(GCObject *, p);
		if(gmark(obj) == GC_STABLE) {
			// do nothing
			// 如果是stable，则不将其放在 gc链 上

		} else if(gmark(obj) == r->currentwhite) {
		//	obj->gch.gclist = r->gclist;
		//	r->gclist = obj;
			list_insert(&r->gclist, p);
		//	list_insert(&r->green, p);
		} else {
			object_release(r, obj);
			g_nFree++;
		}

		g_nGC++;
	}
	//if(r->gray == NULL) {
	if(list_empty(&r->gray)) {
		r->gc_status = GCS_PAUSE;
	}
}

inline void rbtC_mark( void * obj )
{
	g_nMark++;

	GCObject * o = cast(GCObject *, obj);

	if(is_stable(o) && !o->gch.gc_traverse) {
		return;
	}

	if(gmark(o) == o->gch.r->currentwhite) {
		return;
	}

	mark_white(o);

	if(likely(o->gch.gc_traverse)) {
		GCObject ** pp = cast(GCObject **, rbtQ_push(o->gch.r->gc_queue));
		*pp = o;
	}
}

static inline void mark_step( rabbit * r, int count )
{
	while(count-- && !rbtQ_empty(r->gc_queue)) {
		void ** pp = rbtQ_peek(r->gc_queue);
		rbtQ_pop(r->gc_queue);

		GCObject * obj = *pp;

		if(obj->gch.gc_traverse) {
			obj->gch.gc_traverse(obj);
		}
	}

	if(rbtQ_empty(r->gc_queue)) {
		r->gc_status = GCS_SWEEP;
	}
}

void rbtC_step( rabbit * r , int count )
{
	if(r->gc_status == GCS_PAUSE) {
		return rbtC_begin( r );
	}

	if(r->gc_status == GCS_MARK) {
		return mark_step( r, count );
	}

	if(r->gc_status == GCS_SWEEP) {
		return sweep_step( r , count );
	}
}

void rbtC_full_gc( rabbit * r )
{
	if(r->gc_status == GCS_PAUSE) {
		rbtC_begin(r);
	}

	while(r->gc_status == GCS_MARK) {
		mark_step(r, 100000);
	}

	while(r->gc_status == GCS_SWEEP) {
		sweep_step(r, 100000);
	}
}

static int g_last_mem = 0;
static int g_time = 0;
static int g_inc_count = 0;
static int g_pred_mem = 0;
void rbtC_auto_gc( rabbit * r )
{
	int step = r->mem - g_last_mem;
	used(step);
	g_last_mem = r->mem;

	msec_t msec = rbtTime_curr(r);

	// 如果大于predicate memory，进行full gc
	if(r->mem > g_pred_mem) {
		kLOG(r, 0, "[LOG] 内存为 %dM %dK %dB, Full GC, obj:%zu\n", r->mem >> 20, (r->mem >> 10) & 0x3FF, r->mem & 0x3FF, r->obj);
		goto do_gc;
	}

	// 并且10秒，进行一次full gc
//	if(msec - g_time > 10000) {
//		kLOG(r, 0, "[LOG] 10秒没GC了，内存为 %dM %dK %dB, Full GC, obj:%d\n", r->mem >> 20, (r->mem >> 10) & 0x3FF, r->mem & 0x3FF, r->obj);
//		goto do_gc;
//	}

	// 进行少量gc
	g_inc_count++;
	rbtC_step(r, r->obj * min(g_inc_count * 0.1, 0.1));

	return;

do_gc:
	kLOG(r, 0 ,"内存(%d)超出1.5倍，进行一次full_gc" , r->mem);

	g_inc_count = 0;
	if(r->gc_status != GCS_PAUSE) {
		rbtC_full_gc(r);
	}
	rbtC_full_gc(r);
	g_pred_mem = r->mem * 1.5;

	g_time = msec;

	return;

	/*
	// 10M 内不GC
	if(r->mem < 10 * 1024 * 1024) {
		return;
	}

	// 400M+，Full GC
	if(r->mem > 400 * 1024 * 1024) {
		kLOG(r, 0, "[LOG] 内存大于400M，Full GC, obj:%d\n", r->obj);
		rbtM_dump(r);
		rbtC_full_gc(r);
		return;
	}

	// 每次内存减小，GC 次数减少10%
	if(step <= 0) {
		g_inc_count = 0;
		g_step = max(1000, g_step * 0.9);
		goto step;
	}

	if(++g_inc_count > 1) {
		// 2次内存连续增加，GC次数增加1倍(20%)
		g_step = min(g_step * 1.2, r->obj);
		g_inc_count = 0;
	}

step:
	kLOG(r, 0, "[GC] curr step:%d, obj:%d\n", g_step, r->obj);
	rbtC_step(r, g_step);
	*/
}

