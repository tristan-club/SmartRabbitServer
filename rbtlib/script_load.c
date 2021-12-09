#include "object.h"
#include "rabbit.h"
#include "script_load.h"
#include "script_struct.h"
#include "proto.h"
#include "util.h"
#include "string.h"
#include "mem.h"
#include "stream.h"
#include "common.h"
#include "gc.h"

struct read_context {
	rabbit * r;
	const char * p;
	int curr;
	int size;
};

static int 
iload_int(struct read_context * ctx, int * i)
{
	if(ctx->curr + 4 > ctx->size) {
		return -1;
	}

	const char * p = &ctx->p[ctx->curr];

	if(is_bigendian()) {
		*i = (p[0] << 24 & 0xFF000000) | (p[1] << 16 & 0xFF0000) | (p[2] << 8 & 0xFF00) | (p[3] & 0xFF);
	} else {
		*i = (p[0] & 0xFF) | (p[1] << 8 & 0xFF00) | (p[2] << 16 & 0xFF0000) | (p[3] << 24 & 0xFF000000);
	}

	ctx->curr += 4;

	return 0;
}

static int
iload_double(struct read_context * ctx, double * d)
{
	if(ctx->curr + 8 > ctx->size) {
		return -1;
	}
	const char * p = &ctx->p[ctx->curr];

	union {
		char b[8];
		double d;
	} u;

	if(is_bigendian()) {
		u.b[0] = p[7];
		u.b[1] = p[6];
		u.b[2] = p[5];
		u.b[3] = p[4];
		u.b[4] = p[3];
		u.b[5] = p[2];
		u.b[6] = p[1];
		u.b[7] = p[0];

		*d = u.d;
	} else {
		memcpy(d, p, 8);
	}

	ctx->curr += 8;

	return 0;
}

static int
iload_bool(struct read_context * ctx, char * c)
{
	if(ctx->curr + 1 > ctx->size) {
		return -1;
	}
	*c = ctx->p[ctx->curr++];
	return 0;
}

static const TString * 
iload_string(struct read_context * ctx)
{
	int len;
	if(iload_int(ctx, &len) < 0) {
		return NULL;
	}
	if(ctx->curr + len > ctx->size) {
		return NULL;
	}

	const TString * ts = rbtS_init_len(ctx->r, &ctx->p[ctx->curr], len);

	ctx->curr += len;

	return ts;
}

static int
iload_tvalue(struct read_context * ctx, TValue * tv)
{
	int tt;
	if(iload_int(ctx, &tt) < 0) {
		return -1;
	}
	if(tt == TBOOL) {
		char c;
		if(iload_bool(ctx, &c) < 0) {
			return -1;
		}
		setboolvalue(tv, c);
		return 0;
	}
	if(tt == TNUMBER) {
		int i;
		if(iload_int(ctx, &i) < 0) {
			return -1;
		}
		setnumvalue(tv, i);
		return 0;
	}
	if(tt == TFLOAT) {
		double d;
		if(iload_double(ctx, &d) < 0) {
			return -1;
		}
		setfnumvalue(tv, d);
		return 0;
	}
	if(tt == TSTRING) {
		const TString * ts = iload_string(ctx);
		if(!ts) {
			return -1;
		}
		rbtC_stable(cast(GCObject *, ts));
		setstrvalue(tv, ts);
		return 0;
	}

	return -1;
}

static int
iload_proto(struct read_context * ctx, Proto * proto)
{
	int i;
	int sizek;
	if(iload_int(ctx, &sizek) < 0) {
		kLOG(proto->r, 0, "[Error]加载Script (%s) 失败!读取 sizek 失败！\n", rbtS_gets(proto->file));
		goto fail_out;
	}

	proto->sizek = sizek;
	proto->k = RMALLOC(proto->r, TValue, sizek);

	for(i = 0; i < sizek; ++i) {
		if(iload_tvalue(ctx, &proto->k[i]) < 0) {
			kLOG(proto->r, 0, "[Error]加载Script(%s) 失败！读取 常数(%d / %d) 失败！\n", rbtS_gets(proto->file), i, sizek);
			goto fail_out;
		}
	}

	int sizei;
	if(iload_int(ctx, &sizei) < 0) {
		kLOG(proto->r, 0, "[Error]加载Script(%s)失败！读取 sizei 失败！\n", rbtS_gets(proto->file));
		goto fail_out;
	}

	proto->sizei = sizei;
	proto->i = RMALLOC(proto->r, Instruction, sizei);
	proto->line = RMALLOC(proto->r, int, sizei);

	for(i = 0; i < sizei; ++i) {
		if(iload_int(ctx, (int*)(&proto->i[i])) < 0) {
			kLOG(proto->r, 0, "[Error]加载Script(%s)失败！读取 Instruction（%d / %d ) 失败！\n", rbtS_gets(proto->file), i, sizei);
			goto fail_out;
		}
		if(iload_int(ctx, &proto->line[i]) < 0) {
			kLOG(proto->r, 0, "[Error]加载Script(%s)失败！读取 Line（%d / %d ) 失败！\n", rbtS_gets(proto->file), i, sizei);
			goto fail_out;
		}
	}

	int sizep;
	if(iload_int(ctx, &sizep) < 0) {
		kLOG(proto->r, 0, "[Error]加载Script(%s)失败！读取 sizep 失败！\n", rbtS_gets(proto->file));
		goto fail_out;
	}

	proto->sizep = sizep;
	proto->p = RMALLOC(proto->r, Proto *, sizep);

	Proto * tmp = rbtP_proto(proto->r);
	tmp->file = tmp->fname = proto->fname;
	tmp->parent = proto;
	tmp->env = proto->env;

	for(i = 0; i < sizep; ++i) {
		if(iload_proto(ctx, tmp) < 0) {
			kLOG(proto->r, 0, "[Error]加载Script(%s)失败！读取 Proto（%d / %d ) 失败！\n", rbtS_gets(proto->file), i, sizep);
			goto fail_out;
		}

		proto->p[i] = tmp;
	}

	return 0;

fail_out:
	exit(1);
	return -1;
}

Proto *
script_load(Script * S, stream * st, Table * env)
{
	struct read_context ctx;
	ctx.p = st->p;
	ctx.curr = 0;
	ctx.size = st->size;
	ctx.r = S->r;

	Proto * proto = rbtP_proto(S->r);
	proto->file = proto->fname = cast(TString *, st->filename);
	proto->env = env;

	rbtC_stable(cast(GCObject *, proto));
	rbtC_stable(cast(GCObject *, proto->file));
	rbtC_stable(cast(GCObject *, proto->env));

	if(iload_proto(&ctx, proto) < 0) {
		return NULL;
	}

	static char buf[256];
	memset(buf, 0, 256);
	snprintf(buf, 255, "%s.debug", rbtS_gets(proto->file));
	rbtD_proto_to_file(proto, buf);
	
	return proto;
}

static void
isave_int(int fd, int i)
{
	union {
		char b[4];
		int i;
	} u;
	if(is_bigendian()) {
		char * p = (char*)(&i);
		u.b[0] = p[3];
		u.b[1] = p[2];
		u.b[2] = p[1];
		u.b[3] = p[0];
	} else {
		u.i = i;
	}
	write(fd, u.b, 4);
}

static void
isave_bool(int fd, int i)
{
	char b = i;
	write(fd, &b, 1);
}

static void
isave_double(int fd, double d)
{
	union {
		char b[8];
		double d;
	} u;
	if(is_bigendian()) {
		char * p = (char*)(&d);
		u.b[0] = p[7];
		u.b[1] = p[6];
		u.b[2] = p[5];
		u.b[3] = p[4];
		u.b[4] = p[3];
		u.b[5] = p[2];
		u.b[6] = p[1];
		u.b[7] = p[0];
	} else {
		u.d = d;
	}
	write(fd, u.b, 8);
}

static void
isave_string(int fd, const TString * ts)
{
	int len = rbtS_len(ts);
	const char * p = rbtS_gets(ts);
	isave_int(fd, len);
	write(fd, p, len);
}

static void
isave_tvalue(int fd, const TValue * tv)
{
	isave_int(fd, ttype(tv));
	if(ttisnum(tv)) {
		isave_int(fd, numvalue(tv));
	} else if (ttisfnum(tv)) {
		isave_double(fd, fnumvalue(tv));
	} else if (ttisstr(tv)) {
		isave_string(fd, strvalue(tv));
	} else if (ttisbool(tv)) {
		isave_bool(fd, bvalue(tv));
	} else {
		kLOG(NULL, 0, "[Error]保存Script文件出错！尝试写入复杂TValue！\n");
		debug_tvalue_dump(tv);
		exit(1);
	}
}

static void
isave_proto(Script * S, Proto * p, int fd)
{
	int i;
	isave_int(fd, p->sizek);
	for(i = 0; i < p->sizek; ++i) {
		isave_tvalue(fd, &p->k[i]);
	}

	isave_int(fd, p->sizei);
	for(i = 0; i < p->sizei; ++i) {
		isave_int(fd, p->i[i]);
		isave_int(fd, p->line[i]);
	}

	isave_int(fd, p->sizep);
	for(i = 0; i < p->sizep; ++i) {
		isave_proto(S, p->p[i], fd);
	}
}

void
script_save(Script * S, Proto * p, const char * path)
{
	int fd = open(path, O_TRUNC | O_CREAT | O_WRONLY | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
	if(fd < 0) {
		kLOG(p->r, 0, "[Error]保存Script文件至: %s 出错！文件无法打开！\n", path);
		return;
	}

	isave_proto(S, p, fd);

	close(fd);
}
