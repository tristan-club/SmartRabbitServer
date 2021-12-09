#include "db.h"
#include "string.h"
#include "rabbit.h"
#include "table.h"
#include "math.h"
#include "common.h"
#include "mysql.h"
#include "gc.h"
#include "zredis.h"

static void db_mysql_init(rabbit * r)
{
	r->mysql = mysql_init(NULL);
	int b = 1;
	mysql_options(r->mysql, MYSQL_OPT_RECONNECT, &b);
}

int rbtDB_init( rabbit * r, const char * host, const char * user, const char * passwd, const char * db )
{
	if(!get_mysql(r)) {
		db_mysql_init(r);
	}

	MYSQL * mysql = get_mysql(r);

	mysql_options(mysql, MYSQL_INIT_COMMAND, "SET NAMES UTF8");
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8");


	char * sport = strstr( host , ":");

	int port;

	if( sport )
	{
		port = atoi(sport+1);
		sport[0] = '\0';
	}else {
		port = 0;
	}
	
	MYSQL * v = mysql_real_connect(mysql,host,user,passwd,db,port,NULL,0);

	if(!v) {
		kLOG(r, 0, "Failed to connect to database : %s, Error : %s\n",db,mysql_error(mysql));
		return -1;
	}

	kLOG(r, 0, "DB connected port(%d)\n" , port );

	int b = 1;
	mysql_options(r->mysql, MYSQL_OPT_RECONNECT, &b);

	unsigned long version = mysql_get_client_version();
	kLOG(r, 0, "Mysql Version : Lib(%lu), Include(%d)\n", version, MYSQL_VERSION_ID);

	return 0;
}

static int mysql_value_to_tvalue(rabbit * r, int type, void * val, TValue * tv)
{
	switch( type ) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_INT24:
			if(!val) {
				setnumvalue(tv, 0);
				break;
			}

			setnumvalue(tv, stoi((char*)val, strlen((char*)val), NULL));
			break;

		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_BIT:
			if(!val) {
				setnumvalue(tv, 0);
				break;
			}
			 
			setfloatvalue(tv, atof((char*)val));
			break;

		case MYSQL_TYPE_NULL:

			setnilvalue(tv);
			break;

		default:
			if(!val) {
				setnilvalue(tv);
				break;
			}

			setstrvalue(tv, rbtS_new(r, (char*)val));
			break;
	}

	return 0;
}

Table * rbtDB_select( rabbit * r, const char * table, const char * condition, int page, int limit )
{
	static char smt[10240];
	memset(smt,0,10240 * sizeof(char));

	int start = ((page < 1 ? 1 : page ) - 1) * limit;

	int len = snprintf(smt, 10239, "select * from %s where %s limit %d, %d", table, condition, start, limit);

#ifdef DEBUG_SELECT
	kLOG(r, 0, "DB: Select (%s) \n", smt);
	if (r->redis) {
		rLOG(r, "DB: Select (%s) \n", smt);
	}
#endif

	if(len >= 10240) {
		kLOG(r, 0, "[Error]Select too long.Len:%d. Table : %s\n",len, table);
		return NULL;
	}

	MYSQL * mysql = get_mysql(r);
	int v = mysql_real_query(mysql, smt, len);
	if(v) {
		kLOG(r, 0, "[Error]Select Failed. Error : %s\n",mysql_error(mysql));
		return NULL;
	}
#ifdef DEBUG_SELECT
	kLOG(r, 0, "DB : Select Back\n");
	if (r->redis) {
		rLOG(r, "DB : Select Back\n");
	}
#endif

	int nfields = mysql_field_count(mysql);
	if(nfields <= 0) {
		kLOG(r, 0, "[Error]Select Nothing. Error : %s\n",mysql_error(mysql));
		return NULL;
	}

	MYSQL_RES * result = mysql_store_result(mysql);

	if(!result) {
		kLOG(r, 0, "[Error]Select . Result Is Null. Error : %s\n", mysql_error(mysql));
		return NULL;
	}

	MYSQL_FIELD * fields = mysql_fetch_fields(result);

	int i;
	MYSQL_ROW row;
	int nrow = 0;
	Table * tbl = rbtH_init(r,1,4);
	while( (row = mysql_fetch_row(result)) ) {
		TValue * tv = rbtH_setnum(r, tbl, nrow);
		Table * t = rbtH_init(r,1,4);
		settblvalue(tv,t);

		for(i = 0; i < nfields; ++i) {
			tv = rbtH_setstr(r, t, fields[i].name);
			mysql_value_to_tvalue(r, fields[i].type, row[i], tv);
		}
		nrow++;
	}
	mysql_free_result(result);
#ifdef DEBUG_SELECT
	kLOG(r, 0, "DB : Select Process End\n");
	if (r->redis) {
		rLOG(r, "DB : Select Process End\n");
	}
#endif

	return tbl;
}

Table * rbtDB_select_single( rabbit * r, const char * table, const char * condition )
{
	Table * ret = rbtDB_select(r, table, condition, 1, 1);
	if(!ret) {
		return NULL;
	}

	const TValue * tv = rbtH_getnum(r, ret, 0);
	if(!ttistbl(tv)) {
		return NULL;
	}

	return tblvalue(tv);
}

static const char * tvalue_to_string( const TValue * tv )
{
	switch(ttype(tv)) {
		case TNUMBER:
			return itos(numvalue(tv));
			break;

		case TFLOAT:
			return ftos(fnumvalue(tv));
			break;

		case TSTRING:
			return rbtS_gets(strvalue(tv));
			break;

		default:
			break;
	}

	return E_TYPE_STR[ttype(tv)];
}

#define MAX_SMT_LEN	90240
#define MAX_VALUE_LEN	81920

int rbtDB_insert( rabbit * r, const char * table, Table * t , const TString ** inc, const TString ** update )
{
	static char smt[MAX_SMT_LEN];
	memset(smt, 0, MAX_SMT_LEN * sizeof(char));

	static char key[400];
	int nkey = 0;
	memset(key,0,400 * sizeof(char));

	static char val[MAX_VALUE_LEN];
	int nval = 0;
	memset(key, 0, MAX_VALUE_LEN * sizeof(char));

	char * fmt = "insert into %s ( %s ) values ( %s )";

	int idx = -1;
	TValue k,v;
	while( 1 ) {
		idx = rbtH_next(r, t, idx, &k, &v);
		if(idx <= 0 || ttisnil(&v)) {
			break;
		}

		const char * p = tvalue_to_string(&k);
		int len = strlen(p);
		key[nkey++] = '`';
		memcpy(&key[nkey],p,len);
		nkey += len;
		key[nkey++] = '`';
		key[nkey++] = ',';

		if(nkey >= 390) {
			kLOG(r, 0, "[Error]Insert Key Too long.Key Len:%d. Table : %s\n", nkey, table);
			return -1;
		}

		p = tvalue_to_string(&v);
		len = strlen(p);
		val[nval++] = '\'';
		memcpy(&val[nval],p,len);
		nval += len;
		val[nval++] = '\'';
		val[nval++] = ',';

		if(nval >= MAX_VALUE_LEN) {
			kLOG(r, 0, "[Error]Insert Val Too long. Val Len:%d. Table : %s\n", nval, table);
			return -1;
		}
	}

	int len;
	if(nkey <= 0 || nval <= 0) {
		len = snprintf(smt,MAX_SMT_LEN - 1, "insert into %s", table);
	} else {
		key[nkey-1] = '\0';
		val[nval-1] = '\0';
		len = snprintf(smt, MAX_SMT_LEN - 1, fmt, table, key, val);
	}

	if( inc || update ) {
		static char dup_key[MAX_VALUE_LEN];
		static char key_val[MAX_VALUE_LEN];
		memset(dup_key,0,sizeof(dup_key));
		memset(key_val,0,sizeof(key_val));
		int dup_len = snprintf(dup_key, MAX_VALUE_LEN - 1, " on duplicate key update ");
		int old_len = dup_len;
		int i;

		if(inc) {
			for(i = 0; inc[i]; ++i) {
				const TValue * tv = rbtH_getstr(r, t, rbtS_gets(inc[i]));
				if(ttisnumber(tv)) {
					int val = numbervalue(tv);
					int l = snprintf(dup_key + dup_len, MAX_VALUE_LEN - 1 - dup_len, "`%s`=`%s`+%d,",rbtS_gets(inc[i]),rbtS_gets(inc[i]), val);
					dup_len += l;
				}
			}
		}

		if(update) {
			for(i = 0; update[i]; ++i) {
				const TValue * tv = rbtH_getstr(r, t, rbtS_gets(update[i]));
				if(ttisnil(tv)) {
					continue;
				}
				int l = snprintf(dup_key + dup_len, MAX_VALUE_LEN - 1 - dup_len, "`%s`='%s',", rbtS_gets(update[i]),tvalue_to_string(tv));
				dup_len += l;
			}
		}

#ifdef DEBUG_INSERT
		kLOG(r, 0, "Key Val:%s\n", dup_key);
		if (r->redis) {
			rLOG(r, "Key Val:%s\n", dup_key);
		}
#endif

		if(dup_len > old_len && len + dup_len < MAX_SMT_LEN) {
			dup_key[--dup_len] = '\0';
			memcpy(smt+len, dup_key, dup_len);
			len += dup_len;
		}

	}

#ifdef DEBUG_INSERT
	kLOG(r, 0, "Insert : %s\n", smt);
	if (r->redis) {
		rLOG(r, "Insert : %s\n", smt);
	}
#endif

	int result = mysql_real_query(get_mysql(r), smt, len);

	if(result) {
		kLOG(r, 0, "[Error]Insert Error : %s\n", mysql_error(get_mysql(r)));
		return -1;
	}

	return mysql_insert_id(get_mysql(r));
}

static int is_in( rabbit * r, const char * s, TString ** inc )
{
	if(!inc) {
		return 0;
	}

	const TString * ts = rbtS_new(r, s);
	int i;
	for(i = 0; inc[i]; ++i) {
		if( ts == inc[i] ) {
			return 1;
		}
	}

	return 0;
}

int rbtDB_update( rabbit * r, const char * table, const char * condition, Table * t, TString ** inc )
{
	static char smt[102400];
	memset(smt, 0, 102400 * sizeof(char));

	static char key_val[8000];
	memset(key_val, 0, 8000 * sizeof(char));

	int pos = 0;

	int idx = -1;
	TValue key,val;
	while( 1 ) {
		idx = rbtH_next(r, t, idx, &key, &val);
		if(idx < 0 || ttisnil(&val)) {
			break;
		}

		const char * k = tvalue_to_string(&key);
		if(strcmp(k, "id") == 0) {
			// id 忽略
			continue;
		}

		int len = strlen(k);
		key_val[pos++] = '`';
		memcpy(&key_val[pos],k,len);
		pos += len;
		key_val[pos++] = '`';
		key_val[pos++] = '=';

		if(is_in(r, k, inc)) {
			key_val[pos++] = '`';
			memcpy(&key_val[pos],k,len);
			pos += len;
			key_val[pos++] = '`';
			key_val[pos++] = '+';
		}

		const char * v = tvalue_to_string(&val);
		len = strlen(v);
		key_val[pos++] = '\'';
		memcpy(&key_val[pos],v,len);
		pos += len;
		key_val[pos++] = '\'';


		key_val[pos++] = ',';

		if(pos >= 7900) {
			kLOG(r, 0, "Update Key Val Too long. Len : %d. Table : %s\n", pos, table);
			return -1;
		}
	}

	if(pos <= 0) {
		return 0;
	}

	key_val[pos-1] = '\0';

	const char * f = "update %s set %s where %s";
	int smt_len = snprintf(smt, 102400, f, table, key_val, condition);

#ifdef DEBUG_UPDATE
	kLOG(r, 0, "[LOG] Update:%s\n",smt);
	if (r->redis) {
		rLOG(r, "[LOG] Update:%s\n",smt);
	}
#endif

	int result = mysql_real_query(get_mysql(r), smt, smt_len);
	if(result) {
		kLOG(r, 0, "[Error]Update Error:%s\n",mysql_error(get_mysql(r)));
		return -1;
	}

	return mysql_affected_rows(get_mysql(r));
}

int rbtDB_delete( rabbit * r, const char * table, const char * condition )
{
	static char smt[10240];
	memset(smt,0,10240 * sizeof(char));

	int len = snprintf(smt, 10239, "delete from %s where %s", table, condition);

#ifdef DEBUG_DELETE
	kLOG(r, 0, "DB: Delete(%s) \n", smt);
	if (r->redis) {
		rLOG(r, "DB: Delete(%s) \n", smt);
	}
#endif

	if(len >= 10240) {
		kLOG(r, 0, "[Error]Delete too long.Len:%d. Table : %s\n",len, table);
		return -1;
	}

	MYSQL * mysql = get_mysql(r);
	int v = mysql_real_query(mysql, smt, len);
	if(v) {
		kLOG(r, 0, "[Error]Delete Failed. Error : %s\n",mysql_error(mysql));
		return -1;
	}

	return 0;
}

int rbtDB_get_num( rabbit * r, const char * table, const char * condition )
{
	static char smt[1024];
	memset(smt, 0, sizeof(smt));

	int len = snprintf(smt, 1023, "select count(*) as num from `%s` where %s", table, condition);

#ifdef DEBUG_GETNUM
	kLOG(r, 0, "DB : Get Num(%s)\n", smt);
	if (r->redis) {
		rLOG(r, "DB : Get Num(%s)\n", smt);
	}
#endif

	if(len >= 1024) {
		kLOG(r, 0, "[Error]Get Num too long. Len:%d. Table : %s\n", len, table);
		return 0;
	}

	MYSQL * mysql = get_mysql(r);
	int v = mysql_real_query(mysql, smt, len);
	if(v) {
		kLOG(r, 0, "[Error]Get Num Failed(%s)\n", mysql_error(mysql));
		return 0;
	}

	int nfields = mysql_field_count(mysql);
	if(nfields <= 0) {
		kLOG(r, 0, "[Error]Get Num Nothing. Error : %s\n",mysql_error(mysql));
		return 0;
	}

	MYSQL_RES * result = mysql_store_result(mysql);

	if(!result) {
		kLOG(r, 0, "[Error]Get Num . Result Is Null. Error : %s\n", mysql_error(mysql));
		return 0;
	}

	MYSQL_FIELD * fields = mysql_fetch_fields(result);

	MYSQL_ROW row;
	row = mysql_fetch_row(result);

	TValue tv;
	mysql_value_to_tvalue(r, fields[0].type, row[0], &tv);

	mysql_free_result(result);

	return numbervalue(&tv);
}

