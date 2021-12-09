#include "mem.h"
#include "gc.h"

#include "mblock.h"
#include "buffer.h"
#include "rawbuffer.h"
#include "queue.h"
#include "pool.h"

#include "rabbit.h"
#include "object.h"
#include "array.h"
#include "script.h"
#include "stream.h"
#include "script_struct.h"
#include "lexical.h"

#include "remote_call.h"
#include "net_manager.h"
#include "connection.h"
#include "table.h"
#include "packet.h"

#include "script_misc.h"

void * rbtM_realloc( rabbit * r, void * op, size_t osize, size_t nsize )
{
	if(!op && !nsize) { 
		return NULL;
	}

	if(r) {
		r->mem = r->mem - osize + nsize;
		//assert(r->mem > 0);
		if(r->mem > 1600 * 1024 * 1024) {
#ifndef NMEM_PROTECT
			kLOG(r, 0, "内存超出1600M(%zu)\n", nsize);
			rbtM_dump(r);
			((char *)0)[0] = 1;
			exit(1);
			return NULL;
#endif
		}
	} else {

	}

	if(op && !nsize) {
		free(op);
		return NULL;
	}

	void * np;

	if(op) {
		//np = realloc(op,nsize);
		np = malloc(nsize);
		memcpy(np, op, min(nsize, osize));
		free(op);
	} else {
		np = malloc(nsize);
	}
	
	return np;
}

void rbtM_dump(rabbit * r)
{
	if(!r->mem_dump) {
		return;
	}

//	int i, j, k;

	int nrbt = 1, mrbt = sizeof(rabbit), mstbl = r->stbl.size * sizeof(GCObject*);	// rabbit

	// TString
	int nstr = rbtS_debug_count(r), mstr = rbtS_debug_mem(r);
	assert(nstr == r->stbl.used);

	// Table
	int ntbl = rbtH_debug_count(), mtbl = rbtH_debug_mem();

	int nspt = 0, mspt = 0;			// script
	int nspx = 0, mspx = 0;			// script x

	int nstream = 0, mstream = 0;		// stream

	int nud = 0, mud = 0;			// user data

	int nother = 0;

	// NetManager
	int nnet = rbtNet_mgr_count(r);		int mnet = rbtNet_mgr_mem(r);

	// Rpc
	int nrpcp = rbtRpc_debug_param_count();	int mrpcp = rbtRpc_debug_param_mem();

	// Connection
	int ncnn = rbtNet_conn_count();		int mcnn = rbtNet_conn_mem();

	// Packet
	int npkt = rbtM_npkt();			int mpkt = rbtM_mpkt();

	// MBlock
	int nblock = mblock_debug_count();

	// Buffer
	int nbuf = buffer_debug_count();	int mbuf = buffer_debug_mem();

	// Raw buffer
	int nraw = rawbuffer_debug_count();	int mraw = rawbuffer_debug_mem();

	// Queue
	int nque = rbtQ_debug_count();		int mque = rbtQ_debug_mem();

	// Pool
	int npoo = rbtPool_debug_count();	int mpoo = rbtPool_debug_mem();
		
	// Array
	int narr = debug_array_num();		int marr = debug_array_mem();

	// Lexical
	int nlex = script_lex_debug_count();	int mlex = script_lex_debug_mem();

	// Closure
	int ncls = rbtScript_nclosure();	int mcls = ncls * sizeof(Closure);
	int ncls_used = 0;

	// Context
	int nctx = rbtD_ncontext();		int mctx = rbtD_mcontext();
	int nctx_used = 0;

	// Proto
	int npro = rbtScript_proto_count();	int mpro = rbtScript_proto_mem();

	// Misc
	struct misc_debug * misc_d = misc_get_debug();

	// Simple io
	struct io_debug * io_d = io_get_debug();
	int nio = io_d->nio;
	int mio = io_d->mio;

//	GCObject * obj = r->gclist;
	GCObject * obj;

	int loop;
	Script * S;

	struct list_head * head = &r->gclist;
	for(loop = 0; loop < 2; ++loop) {
//	while(obj) {
	struct list_head * p;
	list_foreach(p, head) {
		obj = cast(GCObject*, p);
		switch(obj->gch.tt) {
			case TSTRING:
				break;

			case TTABLE:
				break;

			case TSCRIPT:
				S = cast(Script*, obj);
				nspt++;
				mspt += sizeof(Script) + S->closuresize * sizeof(struct Closure*);
				break;

			case TCLOSURE:
				ncls_used++;
				break;

			case TCONTEXT:
				nctx_used++;
				break;

			case TSCRIPTX:
				nspx++;
			//	mspx += rbtM_scriptX(cast(struct ScriptX *, gco2ud(obj)));
				break;

			case TSTREAM:
				nstream++;
				mstream += sizeof(struct stream);
				break;

			case TUSERDATA:
				nud++;
				break;

			default:
				fprintf(stderr, "Un:%d\n", obj->gch.tt);
				nother++;
				break;
		}

//		obj = obj->gch.gclist;
	}
	head = &r->gray;
//	obj = r->gray;
	}

	fprintf(stderr, "-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-内存Dump！=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	fprintf(stderr, "总Object量：%zu，内存：%d\n", r->obj, r->mem);

	fprintf(stderr, "Rabbit，数量: %d，内存：%d + stbl(%d)\n", nrbt, mrbt, mstbl);

	fprintf(stderr, "字符串，数量: %d，内存：%d\n", nstr, mstr);
	fprintf(stderr, "哈希表，数量: %d，内存：%d\n", ntbl, mtbl);


	fprintf(stderr, "Script，数量：%d，内存：%d\n", nspt, mspt);
	fprintf(stderr, "ScriptX，数量：%d，内存：%d\n", nspx, mspx);
	fprintf(stderr, "Closure，数量：%d(%d), 内存: %d\n", ncls, ncls_used, mcls);
	fprintf(stderr, "Context，数量：%d(%d)，内存：%d\n", nctx, nctx_used, mctx);
	fprintf(stderr, "Proto，数量：%d，内存：%d\n", npro, mpro);
	fprintf(stderr, "Lex，数量：%d，内存：%d\n", nlex, mlex);

	fprintf(stderr, "NetManager，数量：%d，内存：%d\n", nnet, mnet);
	fprintf(stderr, "Rpc param，数量：%d，内存：%d\n", nrpcp, mrpcp);
	fprintf(stderr, "Packet，数量：%d(MBlock：%d)，内存：%d\n", npkt, nblock, mpkt);
	fprintf(stderr, "Connection，数量：%d，内存：%d\n", ncnn, mcnn);

	fprintf(stderr, "Queue，数量：%d，内存：%d\n", nque, mque);
	fprintf(stderr, "Array，数量：%d，内存：%d\n", narr, marr);
	fprintf(stderr, "Pool，数量：%d，内存：%d\n", npoo, mpoo);
	fprintf(stderr, "Buffer，数量：%d，内存：%d\n", nbuf, mbuf);
	fprintf(stderr, "RawBuffer，数量：%d，内存：%d\n", nraw, mraw);
	fprintf(stderr, "Stream，数量: %d，内存：%d\n", nstream, mstream);

	fprintf(stderr, "UserData，数量：%d，内存：%d\n", nud, mud);

	fprintf(stderr, "SleepParam，数量：%d，内存：%d\n", misc_d->nsleep_param, misc_d->msleep_param);

	fprintf(stderr, "SimpleIO，数量：%d，内存：%d\n", nio, mio);

	int other_mem = r->mem - mrbt - mstbl - mstr - mtbl - mspt - mspx - mcls - mctx - mpro - mlex - mnet - mrpcp - mpkt - mcnn - mque - marr - mpoo - mbuf - mraw - mstream -
			misc_d->msleep_param - mio;

	int other_count = r->obj - nrbt - nstr - ntbl - nspt - nspx - ncls_used - nctx_used - npro - nlex - nnet - nrpcp - npkt - ncnn - nque - narr - npoo - nbuf - nraw - nstream - nud -
			 misc_d->nsleep_param - nio;

	fprintf(stderr, "其他，数量：%d(%d)，内存：%d\n\n",
			other_count,
			nother,
			other_mem
	);
	if(r->mem_dump_fun) {
		r->mem_dump_fun(r, other_mem);
	}
	fprintf(stderr, "-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
}

