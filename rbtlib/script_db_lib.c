#include "script_db_lib.h"
#include "script.h"
#include "remote_call.h"
#include "string.h"
#include "table.h"
#include "math.h"
#include "game_inc.h"
#include "packet.h"
#include "gc.h"
#include "rabbit.h"

#ifdef MULTIDB
#include "db_mgr.h"
#endif

static char * cond_from_table( rabbit * r, Table * argv )
{
#define cond_buf_len	1024
	static char cond[cond_buf_len];
	int pos = 0;

	memset(cond, 0, sizeof(cond));

	TValue key, val;
	int idx = -1;
	while(1) {
		idx = rbtH_next(r, argv, idx, &key, &val);
		if(idx < 0) {
			break;
		}
		if(!ttisstr(&key)){
			continue;
		}
		const TString * ts = rbtO_rawToString(r, &val);

		if( pos > 0 ) {
			if(pos + 4 >= cond_buf_len) {
				kLOG(r, 0, "[Error]Cond From Table. Key(%s) is Too long.cond(%s)\n", rbtS_gets(strvalue(&key)), cond);
				return cond;
			}

			memcpy(&cond[pos], "and ", 4);

			pos += 4;
		}

		if(rbtS_len(strvalue(&key)) + rbtS_len(ts) + pos + 6 >= cond_buf_len) {
			kLOG(r, 0, "[Error]Cond From Table. Key(%s) is Too long. cond(%s)\n", rbtS_gets(strvalue(&key)), cond);
			return cond;
		}

		cond[pos++] = '`';

		memcpy(&cond[pos], rbtS_gets(strvalue(&key)), rbtS_len(strvalue(&key)));
		pos += rbtS_len(strvalue(&key));
		cond[pos++] = '`';
		cond[pos++] = '=';
		cond[pos++] = '\'';
		memcpy(&cond[pos], rbtS_gets(ts), rbtS_len(ts));
		pos += rbtS_len(ts);

		cond[pos++] = '\'';
		cond[pos++] = ' ';
	}

	return cond;
}

static int get_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv = rbtScript_top(ctx->S, 0);

	int suc;
	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Base Get Back From DB. Parse Error:Miss suc!\n");
		settblvalue(tv, rbtH_init(r, 1, 1));
		goto bad;
	}

	Table * data = NULL;

	if(suc == DATA_RESULT_NEED_UPDATA) {
		if(rbtP_readTable(pkt, &data) < 0) {
			kLOG(r, 0, "[Error]Base Get Back From DB. Data Is Missing\n");
			settblvalue(tv, rbtH_init(r, 1, 1));
			goto bad;
		}
	}

	if(data) {
		settblvalue(tv, data);
	} else {
		settblvalue(tv, rbtH_init(r, 1, 1));
	}
bad:

	rbtScript_run(ctx->S);

	return 0;
}

static int _base_get( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get. Table name Is Missing\n");
		return 0;
	}

	const TString * table = strvalue(tv);

	const TString * cond;

	tv = rbtScript_top(S, 1);
	if(ttistbl(tv)) {
		char * c = cond_from_table(S->r, tblvalue(tv));
		cond = rbtS_new(S->r, c);
	} else if(ttisstr(tv)) {
		cond = strvalue(tv);
	} else {
		kLOG(S->r, 0, "[Error]DB GET. Condition Is Missing\n");
		return 0;
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, get_back, S->r->data, DATA_GET, "SS", table, cond);

	param->obj = cast(GCObject *, rbtScript_save( S ));

	return VM_RESULT_YIELD;
}

static int get_all_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv = rbtScript_top(ctx->S, 0);

	int suc;
	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Base Get All Back From Data. Parse Error:Miss suc!\n");

		settblvalue(tv, rbtH_init(r, 1, 1));

		goto bad;
	}

	Table * data = NULL;

	if(suc == DATA_RESULT_NEED_UPDATA) {
		if(rbtP_readTable(pkt, &data) < 0) {
			kLOG(r, 0, "[Error]Base Get All Back From Data. Data Is Missing\n");

			settblvalue(tv, rbtH_init(r, 1, 1));

			goto bad;
		}
	}

	if(data) {
		settblvalue(tv, data);
	} else {
		settblvalue(tv, rbtH_init(r, 1, 1));
	}

bad:

	rbtScript_run(ctx->S);

	return 0;
}

static int _base_get_all( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get All. Table Name Is Missing\n");
		return 0;
	}

	const TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);

	const TString * cond;
	if(ttistbl(tv)) {
		char * c = cond_from_table(S->r, tblvalue(tv));
		cond = rbtS_new(S->r, c);
	} else if(ttisstr(tv)) {
		cond = strvalue(tv);
	} else {
		kLOG(S->r, 0, "[Error]DB Get All. Condition Is Missing\n");
		cond = rbtS_new(S->r, "1 limit 1");
	}

	int page, limit;

	tv = rbtScript_top(S, 2);
	if(!ttisnumber(tv)) {
		page = 1;
		limit = 1000;
	} else {
		page = numbervalue(tv);
		tv = rbtScript_top(S, 3);
		if(!ttisnumber(tv)) {
			limit = page;
			page = 1;
		} else {
			limit = numbervalue(tv);
		}
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, get_all_back, S->r->data, DATA_GET_ALL, "SSdd", table, cond, page, limit);

	param->obj = cast(GCObject *, rbtScript_save( S ));
	param->obj2 = cast(GCObject*, table);

	return VM_RESULT_YIELD;
}


static int get_by_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv;
	tv = rbtScript_top(ctx->S, 0);

	int suc;
	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Get By Back From DB. Parse Error:Miss suc\n");
		settblvalue(tv, rbtH_init(r, 1, 1));
		goto bad;
	}

	Table * data = NULL;

	if(suc == DATA_RESULT_NEED_UPDATA) {
		if(rbtP_readTable(pkt, &data) < 0) {
			kLOG(r, 0, "[Error]Get By Back From DB. Data Is Missing\n");
			settblvalue(tv, rbtH_init(r, 1, 1));
			goto bad;
		}
	}

	if(!param->i) {
		tv = cast(TValue *, rbtH_getstr(r, data, "0"));
		if(!ttistbl(tv)) {
			data = NULL;
		} else {
			data = tblvalue(tv);
		}
	}

	tv = rbtScript_top(ctx->S, 0);
	if(data) {
		settblvalue(tv, data);
	} else {
		settblvalue(tv, rbtH_init(r, 1, 1));
	}

bad:


	rbtScript_run(ctx->S);

	return 0;
}

static int _base_get_by_internal( Script * S, int all )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB GET BY. Table Name Is Missing\n");
		return 0;
	}

	const TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get(%s). Field Name Is Missing\n", rbtS_gets(table));
		return 0;
	}

	const TString * field = strvalue(tv);

	tv = rbtScript_top(S, 2);

	if(!ttisnumber(tv) && !ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get(%s). Field Value Must Be Number Or String\n", rbtS_gets(table));
		return 0;
	}

	const char * value;
	if(ttisstr(tv)) {
		value = rbtS_gets(strvalue(tv));
	} else {
		value = ftos(numbervalue(tv));
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, get_by_back, S->r->data, DATA_GET_BY, "sss", rbtS_gets(table), rbtS_gets(field), value);

	param->obj = cast(GCObject *, rbtScript_save(S));
	param->i = all;

	return VM_RESULT_YIELD;
}

static int _base_get_by( Script * S )
{
	return _base_get_by_internal(S, 0);
}

static int _base_get_by_all( Script * S )
{
	return _base_get_by_internal(S, 1);
}

static int get_num_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	int suc, num = 0;

	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Get Num Back. Parse Error:Miss suc!\n");

		goto label_do;
	}

	if(suc != DATA_RESULT_NEED_UPDATA) {
		kLOG(r, 0, "[Error]Get Num Failed\n");
		goto label_do;
	}

	if(rbtP_readInt(pkt, &num) < 0) {
		kLOG(r, 0, "[Error]Get Num. Num Parse Error\n");
		goto label_do;
	}

label_do:
	{
		struct Context * ctx = cast(struct Context *, param->obj);

		rbtScript_resume(ctx->S, ctx);

		TValue * tv = rbtScript_top(ctx->S, 0);
		setnumvalue(tv, num);

		rbtScript_run(ctx->S);
	}

	return 0;
}

static int _base_get_num( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get Num. Table Name Is Missing\n");
		return 0;
	}

	const TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB Get Num. Condition Is Missing\n");
		return 0;
	}

	const TString * cond = strvalue(tv);

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, get_num_back, S->r->data, DATA_GET_NUM, "SS", table, cond);

	param->obj = cast(GCObject *, rbtScript_save( S ));

	return VM_RESULT_YIELD;
}

static int save_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	int suc = DATA_SAVE_ERROR;
	int id = -1;

	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Save Back. Parse Error\n");
	}

	if(suc == DATA_SAVE_OK) {
		if(rbtP_readInt(pkt, &id) < 0) {
			kLOG(r, 0, "[Error]Save Back. ID Is Missing\n");
		}
	}

	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv = rbtScript_top(ctx->S, 0);
	setnumvalue(tv, id);

	rbtScript_run(ctx->S);

	return 0;
}

static int _base_save( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Save. Table Name Is Missing\n");
		return 0;
	}

	TString * tbl_name = strvalue(tv);

	Table * info;
	tv = rbtScript_top(S, 1);
	if(!ttistbl(tv)) {
		kLOG(S->r, 0, "[Error]Save. Info is Missing\n");
		return 0;
	}

	info = tblvalue(tv);

	Table * inc;
	tv = rbtScript_top(S, 2);
	if(!ttistbl(tv)) {
		kLOG(S->r, 0, "[Error]Save. Inc Is Missing\n");
		return 0;
	}
	inc = tblvalue(tv);

	Table * update;
	tv = rbtScript_top(S, 3);
	if(!ttistbl(tv)) {
		kLOG(S->r, 0, "[Error]Save, Update Is Missing\n");
		return 0;
	}

	update = tblvalue(tv);

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, save_back, S->r->data, DATA_SAVE, "Shhh", tbl_name, info, inc, update);

	param->obj = cast(GCObject *, rbtScript_save(S));

	return VM_RESULT_YIELD;
}


static int update_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	int res = 0;
	int suc;
	if(rbtP_readInt(pkt, &suc) < 0 || suc != DATA_SAVE_OK) {
		kLOG(r, 0, "[Error]Update Back. Update Error\n");
		res = -1;
	}

	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv = rbtScript_top(ctx->S, 0);

	if(res < 0) {
		//setnumvalue(tv, res);
		setboolvalue(tv, 0);
	} else {
		setboolvalue(tv, 1);
	}


	rbtScript_run(ctx->S);

	return 0;
}

static int _base_update( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Update. Table Name Is Missing\n");
		return 0;
	}

	TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttistbl(tv)) {
		kLOG(S->r, 0, "[Error]Update. Key -> Value Is Missing\n");
		return 0;
	}

	Table * info = tblvalue(tv);

	tv = rbtScript_top(S, 2);
	const TString * cond;
	if(ttisstr(tv)) {
		cond = strvalue(tv);
	} else if (ttistbl(tv)) {
		const char * p = cond_from_table(S->r, tblvalue(tv));
		cond = rbtS_new(S->r, p);
	} else {
		kLOG(S->r, 0, "[Error]Update. Condition Is Missing\n");
		return 0;
	}

	tv = rbtScript_top(S, 3);
	Table * inc = NULL;
	if(ttistbl(tv)) {
		inc = tblvalue(tv);
	} else {
		inc = rbtH_init(S->r, 1, 1);
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = NULL;
        if(inc) {
		param = rbtRpc_call(S->r, update_back, S->r->data, DATA_UPDATE, "ShSh", table, info, cond, inc);
	} else {
		param = rbtRpc_call(S->r, update_back, S->r->data, DATA_UPDATE, "ShS", table, info, cond);
	}

	param->obj = cast(GCObject*, rbtScript_save(S));

	return VM_RESULT_YIELD;
}

static int delete_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	int res = 1;
	int suc;
	if(rbtP_readInt(pkt, &suc) < 0 || suc != DATA_DEL_OK) {
		kLOG(r, 0, "[Error]Delete Back. Parse Error\n");
		res = -1;
	}

	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv = rbtScript_top(ctx->S, 0);

	setnumvalue(tv, res);

	rbtScript_run(ctx->S);

	return 0;
}

static int _base_delete( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Delete. Tabel Name Is Missing\n");
		return 0;
	}

	TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);

	const TString * cond;

	if(ttisstr(tv)) {
		cond = strvalue(tv);
	} else if (ttistbl(tv)) {
		const char * p = cond_from_table(S->r, tblvalue(tv));
		cond = rbtS_new(S->r, p);
	} else {
		kLOG(S->r, 0, "[Error]Delete. Condition Is Missing\n");
		return 0;
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, delete_back, S->r->data, DATA_DELETE, "SS", table, cond);

	param->obj = cast(GCObject *, rbtScript_save(S));

	return VM_RESULT_YIELD;
}

static int del_by_back( rabbit * r, struct RpcParam * param, Packet * pkt )
{
	int suc = DATA_DEL_ERROR;
	if(rbtP_readInt(pkt, &suc) < 0) {
		kLOG(r, 0, "[Error]Get By Back From DB. Parse Error\n");
	}

	struct Context * ctx = cast(struct Context *, param->obj);

	rbtScript_resume(ctx->S, ctx);

	TValue * tv;
	tv = rbtScript_top(ctx->S, 0);

	if(suc != DATA_DEL_OK) {
		setboolvalue( tv , 0 );
		kLOG(r, 0, "[Error]Get By Back From DB. Error\n");
	} else {
		setboolvalue( tv , 1 );
	}

	rbtScript_run(ctx->S);

	return 0;
}

static int _base_delete_by( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB DelBY. Table Name Is Missing\n");
		return 0;
	}

	const TString * table = strvalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB DelBy(%s). Field Name Is Missing\n", rbtS_gets(table));
		return 0;
	}

	const TString * field = strvalue(tv);

	tv = rbtScript_top(S, 2);

	if(!ttisnumber(tv) && !ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]DB DelBy(%s). Filed Value Must Be Number Or String\n", rbtS_gets(table));
		return 0;
	}

	const char * value;

	if(ttisstr(tv)) {
		value = rbtS_gets(strvalue(tv));
	} else {
		value = ftos(numbervalue(tv));
	}

#ifdef MULTIDB
	rbtDM_switch(S->r);
#endif
	struct RpcParam * param = rbtRpc_call(S->r, del_by_back, S->r->data, DATA_DEL_BY, "sss", rbtS_gets(table), rbtS_gets(field), value);

	param->obj = cast(GCObject *, rbtScript_save( S ));

	return VM_RESULT_YIELD;

}

#ifdef MULTIDB
static int _base_select_db(Script * S)
{
	rabbit * r = S->r;

	const TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		kLOG(r, 0, "[Error]选择数据库出错！需要填写一个名称！\n");
		return 0;
	}

	TString * name = strvalue(tv);
	tv = rbtH_getstr(r, r->data_pool, rbtS_gets(name));
	if(!ttisp(tv)) {
		kLOG(r, 0, "[Error]选择数据库出错！没有这个数据库(%s)！\n", rbtS_gets(name));
		return 0;
	}

	struct DB * db = cast(struct DB *, pvalue(tv));
	r->data = db->c;
	return 0;
}
#endif

int rbtScript_db_init( Script * S )
{
	rbtScript_register(S, "Get",                    _base_get);
	rbtScript_register(S, "GetAll",                 _base_get_all);
	rbtScript_register(S, "GetBy",                  _base_get_by);
	rbtScript_register(S, "GetByAll",               _base_get_by_all);
	rbtScript_register(S, "GetAllBy",               _base_get_by_all);
	rbtScript_register(S, "GetNum",                 _base_get_num);
	rbtScript_register(S, "Save",                   _base_save);
	rbtScript_register(S, "Update",                 _base_update);
	rbtScript_register(S, "DelBy" ,                 _base_delete_by);
	rbtScript_register(S, "Delete",                 _base_delete);
#ifdef MULTIDB
	rbtScript_register(S, "SelectDB",                 _base_select_db);
#endif

	return 0;
}

