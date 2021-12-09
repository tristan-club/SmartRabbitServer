#include "rabbit.h"
#include "mem.h"
#include "connection.h"
#include "table.h"
#include "gc.h"
#include "queue.h"
#include "remote_call.h"
#include "string.h"
#include "syslog.h"

const char * E_TYPE_STR[] = {
	"TNIL",
	"TNUMBER",
	"TBOOL",
	"TFLOAT",
	"TDATE",

	"TGC",
	"TSTRING",
	"TBUFFER",
	"TQUEUE",
	"TTABLE",
	"TPOOL",

	"TSTREAM",
	
	"TPACKET",

	"TEVENTHANDLER",

	"TCONNECTION",

	"TUSERDATA",
	
	"TYPE_END"
};

rabbit * rabbit_init ( )
{
	rabbit * r = RMALLOC(NULL,rabbit,1);

	// gc
	//r->gclist = NULL;
	//r->gray = NULL;
	list_init(&r->gclist);
	list_init(&r->gray);
	list_init(&r->green);

	r->mem = sizeof(rabbit);
	r->obj = 1;
	r->mem_dump = 0;
	r->mem_dump_fun = NULL;

	r->currentwhite = 1;
	r->gc_queue = rbtQ_init(r, sizeof(void *), 32);
	r->gc_status = GCS_PAUSE;

	//  network & event handler
	r->net_mgr = NULL;
	r->conn_broken = NULL;

	// string table 
	r->stbl.table = NULL;
	r->stbl.used = 0;
	r->stbl.size = 0;

	// config
	r->config = rbtH_init(r,1,1);
	rbtC_stable(cast(GCObject *, r->config));
	r->auth = 0;
	r->max_conns = 8888;

	static union {
		char x[2];
		short int i;
	}tt;
	tt.i = 0x0001;
	r->little_endian = tt.x[0];

	r->console = 1;
	openlog(program_name, LOG_CONS | LOG_PID, 0);

	// mysql
	r->mysql = NULL;
	r->data = NULL;
	r->data_pool = rbtH_init(r, 1, 1);
	rbtC_stable(cast(GCObject *, r->data_pool));

	// user data
	r->_G = NULL;

	// remote call
	rbtRpc_init(r);
	r->rpc_process = NULL;

	// time
	gettimeofday(&r->tm, NULL);

	// is client( no rpc )
	r->is_client = 0;

	r->empty_str = rbtS_new(r, "");
	rbtC_stable(cast(GCObject *, r->empty_str));

	// debug
	r->debug_is_script_end = 0;

	return r;
}

msec_t rbtTime_curr(rabbit * r)
{
	struct timeval tm;
	gettimeofday(&tm, NULL);

	msec_t t = (tm.tv_sec - r->tm.tv_sec) * 1000 + (tm.tv_usec - r->tm.tv_usec) * 0.001;
	return t;
}
