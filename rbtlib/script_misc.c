#include "script_misc.h"
#include "mem.h"
#include "rabbit.h"
#include "gc.h"

#define SLEEP_INIT_PARAM 10

static struct misc_debug g_Debug = { 0 };

static LIST_HEAD(sleep_list_busy);
static LIST_HEAD(sleep_list_idle);

struct sleep_param {
	struct list_head node;
	Context * context;
	msec_t timeout;
}init_param[SLEEP_INIT_PARAM];

void rbtScript_sleep(Script * S, int msec)
{
	struct sleep_param *param = NULL;
	struct list_head *it, *tmp;

	if(!list_empty(&sleep_list_idle)) {
		it = list_first_entry(&sleep_list_idle);
		list_del(it);
		param = list_entry(it, struct sleep_param, node);
	} else {
		param = RMALLOC(S->r, struct sleep_param, 1);
		g_Debug.nsleep_param++;
		g_Debug.msleep_param += sizeof(struct sleep_param);
	}

	param->context = rbtScript_save(S);
	param->timeout = msec + rbtTime_curr(S->r);

	list_foreach_safe(it, tmp, &sleep_list_busy) {
		if (list_entry(it, struct sleep_param, node)->timeout > param->timeout) {
			list_insert(it->prev, &param->node);
			return ;
		}
	}
			
	list_insert(sleep_list_busy.prev, &param->node);
}

void rbtScript_sleep_checktimeout(Script * S)
{
	struct list_head *it, *tmp;
	list_foreach_safe(it, tmp, &sleep_list_busy) {
		struct sleep_param *param = list_entry(it, struct sleep_param, node);

		if (rbtTime_curr(S->r) >= param->timeout) {
			struct Context * ctx = cast(struct Context *, param->context);

			rbtScript_resume(ctx->S, ctx);
			rbtScript_run(ctx->S);

			list_del(&param->node);
			list_insert(&sleep_list_idle, &param->node);
		}
		else {
			break;
		}
	}
}

void rbtScript_traverse(rabbit * r)
{
	struct list_head *it, *tmp;
	list_foreach_safe(it, tmp, &sleep_list_busy) {
		rbtC_mark(list_entry(it, struct sleep_param, node)->context);
	}
}

struct misc_debug * misc_get_debug()
{
	return &g_Debug;
}
