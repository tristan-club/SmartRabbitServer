#include "script_base_lib.h"

#include "io.h"
#include "amf.h"

#include "rawbuffer.h"
#include "script.h"
#include "script_struct.h"
#include "pool.h"
#include "table.h"
#include "rabbit.h"
#include "string.h"
#include "string_struct.h"
#include "table_struct.h"
#include "math.h"
#include "md5.h"
#include "packet.h"
#include "rsa.h"
#include "script_misc.h"
#include "gc.h"
#include "php.h"
#include <time.h>

/*#define SLEEP_INIT_PARAM 10

static LIST_HEAD(sleep_list_busy);
static LIST_HEAD(sleep_list_idle);

struct sleep_param {
	struct list_head node;
	Context * context;
	int timeout;
}init_param[SLEEP_INIT_PARAM];*/

static int _base_print( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	time_t t = time(NULL);

	struct tm *current = localtime(&t);

	char * asc_time = asctime(current);

	asc_time[24] = '\0';

	if(ttisstr(tv)) {
		fprintf(stderr, "\"%s\"\t[%s]\n", rbtS_gets(strvalue(tv)) , asc_time );
		return 0;
	}
	if(ttisnum(tv)) {
		fprintf(stderr, "%d(%x)\t[%s]\n", numvalue(tv), numvalue(tv) , asc_time);
		return 0;
	}
	if(ttisfnum(tv)) {
		fprintf(stderr, "%f\t[%s]\n", fnumvalue(tv) , asc_time );
		return 0;
	}
	if(ttisnil(tv)) {
		fprintf(stderr, "Nil\t[%s]\n" , asc_time );
		return 0;
	}
	if(ttisbool(tv)) {
		if(ttistrue(tv)) {
			fprintf(stderr, "Bool(true)\t[%s]\n" ,asc_time );
		} else {
			fprintf(stderr, "Bool(false)\t[%s]\n" , asc_time );
		}
		return 0;
	}
	if(ttisclosure(tv)) {
		fprintf(stderr, "Closure\n");
		return 0;
	}

	debug_tvalue_dump(tv);

	return 0;
}

static int _rand( Script * S )
{
	int ret = rand();

	setnumvalue(rbtScript_top(S, 0), ret);

	return 0;
}

static int _base_sgn( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisnumber(tv)) {
		return 0;
	}
	double x = numbervalue(tv);
	if( x >= 0 ) {
		setnumvalue(tv, 1);
	} else {
		setnumvalue(tv, -1);
	}

	return 0;
}

static int _rand_int( Script * S )
{
	TValue * arg1 = rbtScript_top( S , 0 );
	TValue * arg2 = rbtScript_top( S , 1 );
	int from = numbervalue( arg1 );
	int to = numbervalue( arg2 );
	int gap  = to - from;
	if( gap < 0 )
	{
		setnumvalue(rbtScript_top(S, 0), 0);
		return 0;
	}
	if( gap == 0 )
	{
		setnumvalue(rbtScript_top(S, 0), from);
		return 0;
	}

	int d = rand();
	int r  = from + (int)( d * 1.0f / (RAND_MAX + 1.0) * (gap+1) );
	setnumvalue(rbtScript_top(S, 0), r);
	return 0;
}

static int _eval( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisstr(tv)) {
		setnumvalue(tv, 0);
		return 0;
	}

	TString * str = strvalue(tv);

	const char * buf = rbtS_gets(str);

	setnumvalue(tv, eval(buf));

	return 0;
}

static int _base_unix_timestamp( Script * S )
{
	TValue * tv = rbtScript_top(S , 0);
	time_t t = time(NULL);

	setfnumvalue(tv, t);
	return 0;
}

static int _base_today_time( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	time_t t = time(NULL);
	struct tm * tm = localtime(&t);

	tm->tm_sec = 0;
	tm->tm_min = 0;
	tm->tm_hour = 0;

	t = mktime(tm);

	setfnumvalue(tv, t);

	return VM_RESULT_OK;
}

static int _sqrt( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	
	double d = numbervalue(tv);

	setfnumvalue(tv, sqrt(d));

	return 0;
}

static int _min( Script * S )
{
	const TValue * tv = rbtScript_top(S, 0);
	double a = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	double b = numbervalue(tv);

	setfnumvalue(rbtScript_top(S, 0), min(a, b));

	return 0;
}

static int _max( Script * S )
{
	const TValue * tv = rbtScript_top(S, 0);
	double a = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	double b = numbervalue(tv);

	setfnumvalue(rbtScript_top(S, 0), max(a, b));

	return 0;
}

static int _clamp(Script * S)
{
	const TValue * tv = rbtScript_top(S, 0);
	double x = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	double a = numbervalue(tv);

	tv = rbtScript_top(S, 2);
	double b = numbervalue(tv);

	setfnumvalue(rbtScript_top(S, 0), max(a, min(x, b)));

	return 0;
}

static int _pow(Script * S)
{
	TValue * tv = rbtScript_top(S, 0);
	double x = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	double y = numbervalue(tv);

	setfnumvalue(rbtScript_top(S, 0), pow(x, y));

	return 0;
}

static int script_math_lib_init( Script * S )
{
	TValue * tv = rbtH_setstr(S->r, S->lib, "Math");
	Table * tbl;
	if(!ttistbl(tv)) {
		tbl = rbtH_init(S->r, 1, 1);
		settblvalue(tv, tbl);
	} else {
		tbl = tblvalue(tv);
	}

#define Register( name, func )	{\
	Closure * cl = rbtScript_closure( S );	\
	cl->isC = 1;	\
	cl->u.cf = func;	\
	cl->cf_name = rbtS_new(S->r, name);	\
	setclosurevalue(rbtH_setstr(S->r, tbl, name), cl);	\
}
	Register( "rand", _rand );
	Register( "eval", _eval );
	Register( "IntRand" , _rand_int);
	Register( "sqrt", _sqrt );
	Register( "min", _min );
	Register( "max", _max );
	Register( "clamp", _clamp );
	Register( "pow", _pow);

#undef Register

	return 0;
}

static int _base_replace( Script * S )
{
	return 0;
}

static int _base_at( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]String.at. Is not string\n");
		setnumvalue(tv, 0);
		return 0;
	}

	TString * ts = strvalue(tv);

	tv = rbtScript_top(S, 1);

	if(!ttisnumber(tv)) {
		kLOG(S->r, 0, "[Error]String.at. Position is missing\n");
		setnumvalue(tv, 0);
		return 0;
	}

	int pos = (int)numbervalue(tv);

	if(pos >= ts->len) {
		kLOG(S->r, 0, "[Error]String.at. Position is bigger than len\n");
		setnumvalue(tv, 0);
		return 0;
	}

	setnumvalue(tv, rbtS_gets(ts)[pos]);

	return 0;
}

static int _base_sub( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]String.sub. Is not string\n");
		return 0;
	}

	TString * ts = strvalue(tv);

	tv = rbtScript_top(S, 1);

	if(!ttisnumber(tv)) {
		kLOG(S->r, 0, "[Error]String.sub. Position is missing\n");
		setstrvalue(tv, rbtS_new(S->r, ""));
		return 0;
	}

	int pos = numbervalue(tv);

	if (pos < 0) {
		pos = max(0 ,pos + (int)ts->len);
	}

	if(pos >= ts->len) {
		kLOG(S->r, 0, "[Error]String.sub. Position is bigger than len\n");
		setstrvalue(tv, rbtS_new(S->r, ""));
		return 0;
	}

	tv = rbtScript_top(S, 2);

	int len = -1;

	if(ttisnumber(tv)) {
		len = numbervalue(tv);
	}

	if(len < 0 || len + pos >= ts->len) {
		len = ts->len - pos;
	}

	const char * p = rbtS_gets(ts);

	const TString * sub_str = rbtS_init_len(S->r, &p[pos], len);

	setstrvalue(rbtScript_top(S, 0), sub_str);

	return 0;
}

static int _base_strcmp( Script *S )
{
	TValue * tv1 = rbtScript_top(S, 0);
	TValue * tv2 = rbtScript_top(S, 1);

	if( !ttisstr(tv1) || !ttisstr(tv2)){
		kLOG(S->r, 0, "[Error]Strcmp 调用，传入参数不为string\n" );
		setnilvalue( rbtScript_top(S,0) );
		return 0;
	}

	int r = strcmp( rbtS_gets(strvalue(tv1)) , rbtS_gets(strvalue(tv2)) );

	setnumvalue( rbtScript_top(S, 0) , r );

	return 0;
}

static int _base_strlen(Script *S)
{
	TValue *tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Strlen: Parameter Is not string\n");
		setnumvalue (tv, 0) ;
		return 0 ;
	}

	setnumvalue (tv, rbtS_len(strvalue(tv)));
	return 0 ;
}

static int _base_strlowercase( Script *S )
{
	TValue *tv = rbtScript_top(S, 0);

	if (!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Strlowercase 调用，传入参数不为string\n" );
		return 0;
	}

	setstrvalue (tv, rbtS_lowercase(S->r, strvalue(tv)));

	return 0;
}

static int _base_strreplace ( Script * S )
{
	TValue *tv = rbtScript_top(S, 0);

        if(!ttisstr(tv)) {
                kLOG(S->r, 0, "[Error]Strreplace: Parameter1 Is not string\n");
                setstrvalue(tv, EmptyString(S->r)) ;
                return 0;
        }

        const TString *ts = strvalue(tv);

        /* if either substr or replacement is NULL, duplicate string a let caller handle it */
        tv = rbtScript_top(S, 1);

        if (ttisnil(tv)) {
                return 0 ;
        }

        if (!ttisstr(tv)) {
                kLOG(S->r, 0, "[Error]Strreplace: Parameter2 Is not string\n");
                setstrvalue(rbtScript_top(S, 0), EmptyString(S->r)) ;
                return 0;
        }

        const TString *ts_sub = strvalue(tv);

        tv = rbtScript_top(S, 2);

        if (ttisnil(tv)) {
                return 0 ;
        }

        if (!ttisstr(tv)) {
		kLOG(S->r, 0, "[Error]Strreplace: Parameter3 Is not string\n");
                setstrvalue(rbtScript_top(S, 0), EmptyString(S->r)) ;
                return 0;
        }

        const TString *ts_replace = strvalue(tv);
        const TString *newstr = rbtS_replace(S->r, ts, ts_sub, ts_replace);

        setstrvalue(rbtScript_top(S, 0), newstr);
        return 0;
}

static int script_string_lib_init( Script * S )
{
	TValue * tv = rbtH_setstr(S->r, S->lib, "String");
	Table * tbl;
	if(!ttistbl(tv)) {
		tbl = rbtH_init(S->r, 1, 1);
		settblvalue(tv, tbl);
	} else {
		tbl = tblvalue(tv);
	}

#define Register( name, func )	{\
	Closure * cl = rbtScript_closure( S );	\
	cl->isC = 1;	\
	cl->u.cf = func;	\
	cl->cf_name = rbtS_new(S->r, name);	\
	setclosurevalue(rbtH_setstr(S->r, tbl, name), cl);	\
}
	Register( "replace", _base_replace );
	Register( "at", _base_at );
	Register( "sub", _base_sub );

#undef Register

	return 0;
}
static int _base_ret( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	void(*fun)(rabbit *, struct ExtParam *, TValue *) = NULL;

	if(S->ctx->f) {
		fun = S->ctx->f;

		struct ExtParam * ep = rbtPool_at(S->ext_params, S->ctx->param);

		fun(S->r, ep, tv);

		if(ep) {
			rbtPool_free(S->ext_params, S->ctx->param);
		}
	}

	S->ctx->f = NULL;
	S->ctx->param = -1;

	return 0;
}

static int _base_strtotime( Script * S )
{
	TValue *arg0 = rbtScript_top(S, 0);

	if( !ttisstr(arg0) ){
		kLOG(S->r, 0, "[Error]strtotime! 第一个参数必须为字符串\n");
		setnilvalue(arg0);
		return 0;
	}

	TString *str = strvalue(arg0);

	struct tm tm_time;
	
	strptime(rbtS_gets(str), "%Y-%m-%d %H:%M:%S", &tm_time);

	setnumvalue(arg0, mktime(&tm_time));

	return 0;
}

static int _base_date( Script * S )
{
#define MAX_TIME_LEN	64
	static char buf[MAX_TIME_LEN];
	memset(buf, 0, sizeof(buf));

	static int year_pos[MAX_TIME_LEN];
	memset(year_pos, 0, sizeof(year_pos));

	int nyear = 0;

	int pos = 0;

	TValue * tv = rbtScript_top(S, 0);

	const char * fmt;

	time_t tim;

	if(!ttisstr(tv)) {
		fmt = "ymd h:i:s";
	} else {
		fmt = rbtS_gets(strvalue(tv));
	}

	tv = rbtScript_top(S, 1);
	if(!ttisnumber(tv)) {
		tim = time(NULL);
	} else {
		tim = numbervalue(tv);
	}

	struct tm * tm = localtime(&tim);

	char c = *fmt;

	int year = tm->tm_year + 1900;

	int mon = tm->tm_mon + 1;

	int hour = tm->tm_hour;

	int wday = tm->tm_wday + 1;

	char next;

	int add;

	int i;

	char * p;

	while(c) {
		switch(c) {
			case 'y':
				year_pos[nyear++] = pos;
				buf[pos++] = year / 1000 + '0';
				buf[pos++] = (year / 100) % 10 + '0';
				buf[pos++] = (year / 10) % 10 + '0';
				buf[pos++] = (year % 10) + '0';
				break;

			case 'm':
				next = *(fmt + 1);	
				if(next && next == '+') {
					fmt++;
					add = 1;
					next = *(fmt + 1);
					if(next >= '0' && next <= '9') {
						fmt++;
						add = next - '0';
					}
					mon += add;
					if(mon > 12) {
						mon -= 12;
						year ++;
						for(i = 0; i < nyear; ++i) {
							p = &buf[year_pos[i]];
							p[0] = year / 1000 + '0';
							p[1] = (year / 100) % 10 + '0';
							p[2] = (year / 10) % 10 + '0';
							p[3] = (year % 10) + '0';
						}
					}
				}
				buf[pos++] = mon / 10 + '0';
				buf[pos++] = mon % 10 + '0';
				break;

			case 'd':
				buf[pos++] = tm->tm_mday / 10 + '0';
				buf[pos++] = tm->tm_mday % 10 + '0';
				break;

			case 'H':
			case 'h':
				buf[pos++] = hour / 10 + '0';
				buf[pos++] = hour % 10 + '0';
				break;

			case 'i':
				buf[pos++] = tm->tm_min / 10 + '0';
				buf[pos++] = tm->tm_min % 10 + '0';
				break;

			case 's':
				buf[pos++] = tm->tm_sec / 10 + '0';
				buf[pos++] = tm->tm_sec % 10 + '0';
				break;

			case 'w':
				buf[pos++] = wday + '0';
				break;

			default:
				buf[pos++] = c;
				break;
		}

		if(pos >= MAX_TIME_LEN - 4) {
			break;
		}

		c = *(++fmt);
	}

	buf[MAX_TIME_LEN - 1] = 0;

	const TString * ts = rbtS_new(S->r, buf);

	setstrvalue(rbtScript_top(S, 0), ts);

#undef MAX_TIME_LEN

	return 0;
}

static int _base_php_serialize( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	rawbuffer * buf = rawbuffer_init( S->r, 256 );
	php_serialize(S->r, buf, tv);

	const TString * ts = rbtS_init_len(S->r, buf->buf, buf->pos);

	setstrvalue(tv, ts);

	rawbuffer_dealloc(S->r, buf);

	return 0;
}

static int _base_php_deserialize( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		return 0;
	}

	rawbuffer * buf = rawbuffer_init( S->r, 0 );
	buf->buf = cast(char*, rbtS_gets(strvalue(tv)));
	buf->len = rbtS_len(strvalue(tv));
	buf->pos = 0;

	TValue val;

	setnilvalue(&val);
	if(buf->len > 0) {
		php_deserialize(S->r, buf, &val);
	}

	setvalue(tv, &val);

	buf->buf = NULL;

	rawbuffer_dealloc(S->r, buf);

	return 0;
}
static int _base_math_num( Script * S)
{
	TValue * tv = rbtScript_top(S,0);

	if( ttisnumber(tv ))
	{
		return 0 ;
	}
	setnumvalue( tv , 0 );
	return 0;
}

static int _base_int( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttisnumber(tv)) {
		int n = (int)numbervalue(tv);
		setnumvalue(tv, n);
	} else
	if(ttisstr(tv)) {
		TString * ts = strvalue(tv);
		setnumvalue(tv, (int)stof(gets(ts), ts->len, NULL));
	} else {
		setnumvalue(tv, 0);
	}

	return 0;
}

static int _base_float( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttisnumber(tv)) {
		double n = numbervalue(tv);
		setfnumvalue(tv, n);
	} else
	if(ttisstr(tv)) {
		TString * ts = strvalue(tv);
		setfnumvalue(tv, stof(gets(ts), ts->len, NULL));
	} else {
		setfnumvalue(tv, 0);
	}

	return 0;
}

static int _base_ceil( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttisnumber(tv)) {
		int n = (int)ceil(numbervalue(tv));
		setnumvalue(tv, n);
	} else {
		setnumvalue(tv, 0);
	}

	return 0;
}

/*
static int _base_mkdir( Script * S )
{
	TValue * tv = rbtScript_top(S,0);
	if(! ttisstr(tv )) {
		return 0;
	}
	const char *path = rbtS_gets( strvalue(tv) );
	if( mkdir(path ,0755) < 0 )
	{
		perror("mkdir error");
	}
	return 0;
}*/

static int _base_hook( Script * S )
{
	return 0;
}

static int _base_isnum( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttisnumber(tv)) {
		setboolvalue(tv, 1);
	} else {
		setboolvalue(tv, 0);
	}

	return 0;
}

static int _base_istable( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttistbl(tv)) {
		setboolvalue(tv, 1);
	} else {
		setboolvalue(tv, 0);
	}

	return 0;
}

static int _base_tonum( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(ttisnumber(tv)) {
		return 0;
	}

	if(ttisstr(tv)) {
		TString * ts = strvalue(tv);
		setfnumvalue(tv, stof(gets(ts), ts->len, NULL));
		return 0;
	}

	setnumvalue(tv, 0);

	return 0;
}

static int _base_md5( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		setnilvalue(tv);
		return 0;
	}

	TString * ts = strvalue(tv);
	const char * p = rbtS_gets(ts);

	struct MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, p, ts->len);
	MD5Final(&ctx);

	static char HEX[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	char buf[33];
	int i;
	for(i = 0;i < 16; ++i) {
		buf[2 * i] = HEX[(ctx._digest[i]) >> 4];
		buf[2 * i + 1] = HEX[(ctx._digest[i]) & 0xF];
	}

	buf[32] = 0;

	const TString * md5 = rbtS_new(S->r, buf);

	setstrvalue(tv, md5);

	return 0;
}

static int _base_max( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double a = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double b = numbervalue(tv);

	if(a > b) {
		setfnumvalue(rbtScript_top(S, 0), a);
		return 0;
	}

	setfnumvalue(rbtScript_top(S, 0), b);
	return 0;
}

static int _base_abs( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double a = numbervalue(tv);

	if(a >= 0) {
		setfnumvalue(rbtScript_top(S, 0), a);
		return 0;
	}

	setfnumvalue(rbtScript_top(S, 0), -a);
	return 0;
}

static int _base_min( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double a = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double b = numbervalue(tv);

	if(a > b) {
		setfnumvalue(rbtScript_top(S, 0), b);
		return 0;
	}

	setfnumvalue(rbtScript_top(S, 0), a);

	return 0;
}

static int _base_clamp( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double x = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisnumber(tv)) {
		return 0;
	}

	double a = numbervalue(tv);

	tv = rbtScript_top(S, 2);
	if(!ttisnumber(tv)) {
		return 0;
	}
	double b = numbervalue(tv);

	if(x < a) {
		x = a;
	}
	if(x > b) {
		x = b;
	}

	setfnumvalue(rbtScript_top(S, 0), x);

	return 0;
}

static int _base_split( Script * S )
{
	TValue * tv = rbtScript_top(S, 0);

	Table * table = rbtH_init(S->r, 1, 1);

	if(!ttisstr(tv)){
		settblvalue(rbtScript_top(S, 0), table);
		return 0;
	}
	
	TString * de = strvalue(tv);

	const char * delimiter = rbtS_gets(de);

	tv = rbtScript_top(S, 1);

	if(!ttisstr(tv)){
		settblvalue(rbtScript_top(S, 0), table);
		return 0;
	}

	TString * src = strvalue(tv);

	const char * Src = rbtS_gets(src);

	char * dupSrc = strdup(Src);

	char * pch = strtok( dupSrc, delimiter);

	int at = 0;

	while ( pch != NULL )
	{
		setstrvalue(rbtH_setnum(S->r, table, at), rbtS_new(S->r, pch));
		pch = strtok(NULL, delimiter);
		at++;
	}

	free(dupSrc);

	settblvalue(rbtScript_top(S, 0), table);

	return 0;
}

static int _base_die( Script * S )
{
	/*
	const TString * fname = S->ctx->cl->caller->u.p->fname;
	const TString * file = S->ctx->cl->caller->u.p->file;
	int line = S->ctx->cl->caller->u.p->line[S->ctx->cl->caller->pc] + 1;

	fprintf(stderr, "Die At(%s:%s:%d)\n", rbtS_gets(file), rbtS_gets(fname), line);
	exit(1);*/
	rbtScript_die(S);
	return 0;
}


static int _base_get_msec(Script * S)
{
	struct timeval tm;
	gettimeofday(&tm, NULL);

	rabbit * r = S->r;

	int t = (tm.tv_sec - r->tm.tv_sec) * 1000 + (tm.tv_usec - r->tm.tv_usec) * 0.001;

	setnumvalue(rbtScript_top(S, 0), t);

	return 0;
}

static int _base_export(Script * S)
{
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisnumber(tv)) {
		kLOG(S->r, 0, "[Error]Script export Error! 第一个参数必须为 fun\n");
		_base_die(S);
		return 0;
	}

	int fun = numbervalue(tv);

	tv = rbtScript_top(S, 1);
	if(!ttisclosure(tv)) {
		kLOG(S->r, 0, "[Error]Script export Error! 第二个参数必须为 closure\n");
		_base_die(S);
		return 0;
	}

	Closure * cl = closurevalue(tv);

	rabbit * r = S->r;
	tv = cast(TValue*, rbtH_getnum(r, S->_export, fun));
	if(!ttisnil(tv)) {
		kLOG(S->r, 0, "[Error]Script export Error! 已经有这个 fun(%d)\n", fun);
		_base_die(S);
		return 0;
	}

	setclosurevalue(rbtH_setnum(r, S->_export, fun), cl);
	return 0;
}

static int _base_read_int_amf3(Script * S)
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisp(tv)) {
		kLOG(S->r, 0, "[Error]Script read int amf3 error!第一个参数必须是Packet！\n");
		return 0;
	}
	Packet * pkt = pvalue(tv);
	int i;
	if(rbtP_readIntAMF3(pkt, &i) < 0) {
		kLOG(S->r, 0, "[Error]Script read int amf3 error! 读取错误！\n");
		return 0;
	}

	setnumvalue(tv, i);
	return 0;
}

static int _base_read_double(Script * S)
{
	TValue * tv = rbtScript_top(S, 0);
	if(!ttisp(tv)) {
		kLOG(S->r, 0, "[Error]Script read double error!第一个参数必须是Packet！\n");
		return 0;
	}
	Packet * pkt = pvalue(tv);
	double d;
	if(rbtP_readDouble(pkt, &d) < 0) {
		kLOG(S->r, 0, "[Error]Script read double error! 读取错误！\n");
		return 0;
	}

	setfnumvalue(tv, d);
	return 0;
}

static int _base_rsa_decode(Script * S ){
	return 0;
	/*
	TValue * tv = rbtScript_top(S, 0);

	if(!ttisstr(tv)) {
		setnilvalue( rbtScript_top(S, 0) );
		return 0;
	}

	TString * ts = strvalue(tv);

	const char * p = rbtS_gets(ts);

	int result_len = -1;

	unsigned char * r = rsa_decode( cast(unsigned char *, p) , strlen(p) , &result_len );
	
	if( !r || result_len < 0){
		setnilvalue( rbtScript_top(S ,0) );
		return 0;
	}

	r[result_len] = '\0';

	setstrvalue( rbtScript_top(S ,0) , rbtS_new(S->r, cast(char *, r)) );

	free(r);
	

	return 0;
	*/
}

static int _base_sleep(Script * S)
{
	TValue *tv = rbtScript_top(S, 0);
	if (!ttisnumber(tv)) {
		kLOG(S->r, 0, "[Error]Sleeping Time Is Missing.\n");
		return 0;
	}

	rbtScript_sleep(S, numbervalue(tv));

	/*struct sleep_param *param = NULL;
	struct list_head *it, *tmp;

	if(!list_empty(&sleep_list_idle)) {
		it = list_first_entry(&sleep_list_idle);
		list_del(it);
		param = list_entry(it, struct sleep_param, node);
	} else {
		param = RMALLOC(S->r, struct sleep_param, 1);
	}

	param->context =  rbtScript_save(S);
	param->timeout = numbervalue(tv) + rbtTime_curr(S->r);

	list_foreach_safe(it, tmp, &sleep_list_busy) {
		if (list_entry(it, struct sleep_param, node)->timeout > param->timeout) {
			list_insert(it->prev, &param->node);
			break;
		}
	}*/



	return VM_RESULT_YIELD;	
}

/*void rbtW_sleep_checktimeout(rabbit *r)
{
	struct list_head *it, *tmp;
	list_foreach_safe(it, tmp, &sleep_list_busy) {
		struct sleep_param *param = list_entry(it, struct sleep_param, node);

		if (rbtTime_curr(r) >= param->timeout) {
			struct Context * ctx = cast(struct Context *, param->context);

			rbtScript_resume(ctx->S, ctx);
			rbtScript_run(ctx->S);

			list_del(&param->node);
			list_insert(&sleep_list_idle, &param->node);
		}
		else {
			rbtC_mark(param->context);
		}
	}
	for ( ; (tmp = it->next) && (it != &sleep_list_busy) ; it= tmp) {
		struct sleep_param *param = list_entry(it, struct sleep_param, node);
		rbtC_mark(param->context);
	}
}*/

static int _base_klog( Script * S )
{
	rabbit *r = S->r;
	TValue *tv = rbtScript_top(S, 0);

	if (!ttisnumber(tv)) {
		fprintf(stderr, "[Error]Klog Pid Is Missing.\n");
		return 0;
	}
	int pid = numvalue(tv);

	tv = rbtScript_top(S, 1);
	if(ttisstr(tv)) {
		kLOG(r, pid, "[Script]\"%s\"\n", rbtS_gets(strvalue(tv)));
		return 0;
	}
	if(ttisnum(tv)) {
		kLOG(r, pid, "[Script]%d(%x)\n", numvalue(tv), numvalue(tv));
		return 0;
	}
	if(ttisfnum(tv)) {
		kLOG(r, pid, "[Script]%f\n", fnumvalue(tv));
		return 0;
	}
	if(ttisnil(tv)) {
		kLOG(r, pid, "[Script]Nil\n");
		return 0;
	}
	if(ttisbool(tv)) {
		if(ttistrue(tv)) {
			kLOG(r, pid, "[Script]Bool(true)\n");
		} else {
			kLOG(r, pid, "[Script]Bool(false)\n");
		}
		return 0;
	}
	if(ttisclosure(tv)) {
		kLOG(r, pid, "[Script]Closure\n");
		return 0;
	}
	if (ttistbl(tv)) {
		rbtLog_table(r, tblvalue(tv), pid);
		return 0;
	}

	kLOG(r, pid, "[Error]Klog Content Is Missing or Not Correct.\n");

	return 0;
}

/*
 *	Debug
 */
static int _base_debug_encode_size(Script * S)
{
	rabbit * r = S->r;
	TValue * tv = rbtScript_top(S, 0);
	if(!ttistbl(tv)) {
		kLOG(r, 0, "[Warning]%s:, 第一个参数不是Table！\n", __FUNCTION__);
		setnumvalue(tv, 0);
		return 0;
	}

	struct i_io * io = io_create(r);
	if(rbtAMF_encode(r, tv, io) < 0) {
		kLOG(r, 0, "[Warning]%s:, 序列号失败！\n", __FUNCTION__);
		setboolvalue(tv, 0);
		io_destroy(io);
		return 0;
	}
	setnumvalue(tv, io->size(io));
	io_destroy(io);
	return 0;
}

static int _base_debug_mem_dump(Script * S)
{
	rabbit * r = S->r;

	TValue * tv = rbtScript_top(S, 0);

	rbtM_dump(r);	

	return 0;
}

int rbtScript_lib( Script * S )
{
	rbtScript_register(S, "print", _base_print);
	rbtScript_register(S, "trace", _base_print);
	rbtScript_register(S, "Time", _base_unix_timestamp);
	rbtScript_register(S, "Ret", _base_ret);
	rbtScript_register(S, "Date", _base_date);
	rbtScript_register(S, "strtotime", _base_strtotime);
	rbtScript_register(S, "TodayTime", _base_today_time);
	rbtScript_register(S, "php_serialize", _base_php_serialize);
	rbtScript_register(S, "php_deserialize", _base_php_deserialize);
	rbtScript_register(S ,"Num" , _base_math_num);
	rbtScript_register(S, "int", _base_int);
	rbtScript_register(S, "float", _base_float);
	rbtScript_register(S, "ceil", _base_ceil);
	rbtScript_register(S, "Hook", _base_hook);
	rbtScript_register(S, "IsNum", _base_isnum);
	rbtScript_register(S, "ToNum", _base_tonum);
	rbtScript_register(S, "IsTable", _base_istable);
	rbtScript_register(S, "MD5", _base_md5);
	rbtScript_register(S, "Max", _base_max);
	rbtScript_register(S, "Min", _base_min);
	rbtScript_register(S, "Clamp", _base_clamp);
	rbtScript_register(S, "Abs", _base_abs);
	rbtScript_register(S, "Split", _base_split);
	rbtScript_register(S, "strcmp", _base_strcmp);
	rbtScript_register(S, "Sgn", _base_sgn);
	rbtScript_register(S, "Die", _base_die);
	rbtScript_register(S, "GetMSec", _base_get_msec);
	rbtScript_register(S, "export", _base_export);
	rbtScript_register(S, "ReadIntAmf3", _base_read_int_amf3);
	rbtScript_register(S, "ReadDouble", _base_read_double);

	rbtScript_register(S, "RSADecode" , _base_rsa_decode );

	rbtScript_register(S, "Strlen", _base_strlen) ;
	rbtScript_register(S, "Strlowercase", _base_strlowercase);
        rbtScript_register(S, "Strreplace", _base_strreplace);

	rbtScript_register(S, "Sleep", _base_sleep);
	rbtScript_register(S, "Klog", _base_klog);
	rbtScript_register(S, "kLOG", _base_klog);

	rbtScript_register(S, "debug_encode_size", _base_debug_encode_size);
	rbtScript_register(S, "debug_mem_dump", _base_debug_mem_dump);

	script_math_lib_init( S );

	script_string_lib_init( S );

	return 0;
}

