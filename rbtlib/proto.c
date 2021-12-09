#include "proto.h"
#include "array.h"
#include "mem.h"
#include "gc.h"
#include "table.h"

#include "object.h"
#include "rabbit.h"

#include "list.h"

#include "opcode.h"
#include "string.h"

static LIST_HEAD(g_ProtoLink);
static int g_nProto = 0;

/*
static void traverse( GCObject * obj )
{
	Proto * p = cast(Proto *, obj);

	if(p->file) {
		rbtC_mark(cast(GCObject *, p->file));
	}

	if(p->fname) {
		rbtC_mark(cast(GCObject *, p->fname));
	}

	if(p->h) {
		rbtC_mark(cast(GCObject *, p->h));
	}

	if(p->env) {
		rbtC_mark(cast(GCObject *, p->env));
	}

	int i;
	for(i = 0;i < p->sizek; ++i) {
		if(is_collectable(&p->k[i])) {
			rbtC_mark(gcvalue(&p->k[i]));
		}
	}

	for(i = 0;i < p->sizep; ++i) {
		rbtC_mark(p->p[i]);
	}

	for(i = 0;i < p->sizelocvars; ++i) {
		rbtC_mark(cast(GCObject *, p->locvars[i].name));
	}

	for(i = 0;i < p->sizeuv; ++i) {
		rbtC_mark(cast(GCObject *, p->upvalues[i]));
	}
}

static void release( GCObject * obj )
{
	Proto * p = cast(Proto *, obj);

	RFREEVECTOR(p->r, p->i, p->sizei);
	RFREEVECTOR(p->r, p->line, p->sizei);
	RFREEVECTOR(p->r, p->locvars, p->sizelocvars);
	RFREEVECTOR(p->r, p->upvalues, p->sizeuv);
	RFREEVECTOR(p->r, p->p, p->sizep);
	RFREEVECTOR(p->r, p->k, p->sizek);
	RFREE(p->r, p);


	fprintf(stderr, "Release Proto(%s)!\n", rbtS_gets(p->fname));
}*/

Proto * rbtP_proto( rabbit * r )
{
	Proto * p = RMALLOC(r, Proto, 1);

	p->r = r;

	p->h = rbtH_init(r,1,1);
	rbtH_weak(p->h);
	rbtC_stable(cast(GCObject *, p->h));

	p->env = NULL;

	p->k = NULL;
	p->sizek = 0;

	p->i = NULL;
	p->sizei = 0;
	p->line = NULL;

	p->locvars = NULL;
	p->sizelocvars = 0;

	p->upvalues = NULL;
	p->sizeuv = 0;

	p->p = NULL;
	p->sizep = 0;
	
	p->file = NULL;	// 文件名
	p->fname = NULL;// 函数名

	p->parent = NULL;

	list_init(&p->link);
	list_insert(&g_ProtoLink, &p->link);

	// statistic
	r->obj++;
	g_nProto++;

	return p;
}

int rbtD_proto_mem(Proto * p)
{
	return sizeof(Proto) + p->sizek * sizeof(TValue) + p->sizei * sizeof(Instruction) + p->sizei * sizeof(int) + p->sizelocvars * sizeof(LocVar) +
	       	p->sizeuv * sizeof(TString*) + p->sizep * sizeof(struct Proto*);
}

int rbtScript_proto_count()
{
	return g_nProto;
}

int rbtScript_proto_mem()
{
	int mem = 0;

	struct list_head * h;
	struct Proto * p;
	list_foreach(h, &g_ProtoLink) {
		p = list_entry(h, struct Proto, link);
		mem += rbtD_proto_mem(p);
	}

	return mem;
}

void rbtD_proto( Proto * p )
{
	if(!p) {
		return;
	}

	int i;

	fprintf(stderr,"Local Protos(%zu):\n",p->sizep);
	for(i = 0; i < p->sizep; ++i) {
		rbtD_proto( p->p[i] );
	}

	fprintf(stderr,"Local Variables(%zu):\n",p->sizelocvars);
	for(i = 0; i < p->sizelocvars; ++i) {
		if(p->locvars[i].name) {
			fprintf(stderr,"<%d> %s\n",i,rbtS_gets(p->locvars[i].name));
		} else {
			fprintf(stderr,"<%d> NULL\n",i);
		}
	}

	fprintf(stderr,"Constant(%zu):\n",p->sizek);
	for(i = 0; i < p->sizek; ++i) {
		if(ttisstr(&p->k[i])) {
			fprintf(stderr,"<%d> %s\n",i,rbtS_gets(strvalue(&p->k[i])));
		} else {
			fprintf(stderr,"<%d> %f\n",i,fnumvalue(&p->k[i]));
		}
	}

	fprintf(stderr,"Instructions(%zu):\n",p->sizei);
	for(i = 0; i < p->sizei; ++i) {
		Instruction ii = p->i[i];
		fprintf(stderr,"<%d:%d> %s, RA(%d), RB(%d), RC(%d)\n",p->line[i],i,OpCode_Str[GetOP(ii)],GetA(ii),GetB(ii),GetC(ii));
	}

	fprintf(stderr,"---\n");
}

/*
static char * _inner_str( const char * fmt, ... )
{
	static char buf[1024];
	memset(buf, 0, sizeof(buf));

	va_list va;
	va_start(va, fmt);
	snprintf(buf, 1023, fmt, va);
	va_end(va);

	return buf;
}*/

static void _inner_write( int fd, const char * fmt, ... )
{
	static char buf[1024];
	memset(buf, 0, sizeof(buf));

	va_list va;
	va_start(va, fmt);
	int len = vsnprintf(buf, 1023, fmt, va);
	va_end(va);

	write(fd, buf, len);
}

static void _init_dump( Proto * p, int fd )
{
	int i;
	_inner_write(fd, "Local Protos<%d>:\n\n", p->sizelocvars);

	for(i = 0; i < p->sizep; ++i) {
		_inner_write(fd, "%dth Proto\n", i+1);
		_init_dump( p->p[i], fd );
	}
	_inner_write(fd, "\nLocal Variables(%d):\n", p->sizelocvars);
	for(i = 0; i < p->sizelocvars; ++i) {
		if(p->locvars[i].name) {
			_inner_write(fd, "<%d> %s \n", i, rbtS_gets(p->locvars[i].name));
		} else {
			_inner_write(fd, "<%d> %s \n", i, "null");
		}
	}

	_inner_write(fd, "\nConstant(%d): \n", p->sizek);
	for(i = 0; i < p->sizek; ++i) {
		if(ttisstr(&p->k[i])) {
			_inner_write(fd, "<%d> %s\n", i, rbtS_gets(strvalue(&p->k[i])));
		} else {
			_inner_write(fd, "<%d> %f\n", i, fnumvalue(&p->k[i]));
		}
	}

	_inner_write(fd, "\nInstructions(%d):\n", p->sizei);
	for(i = 0; i < p->sizei; ++i) {
		Instruction ii = p->i[i];
		_inner_write(fd,"<%d> %s, RA(%d), RB(%d), RC(%d)\n",i,OpCode_Str[GetOP(ii)],GetA(ii),GetB(ii),GetC(ii));
	}

	_inner_write(fd, "---\n\n\n");
}

void rbtD_proto_to_file( Proto * p, const char * fname )
{
	int fd = open(fname, O_TRUNC | O_CREAT | O_WRONLY | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
	if(fd < 0) {
		fprintf(stderr, "Proto Dump File(%s) Open Failed\n", fname);
		return;
	}

	_init_dump( p, fd );
}

