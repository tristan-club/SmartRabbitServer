#include "string.h"

#include "rabbit.h"
#include "gc.h"
#include "mem.h"

#include "string_struct.h"
#include "list.h"
#include "common.h"

#include "string_for_vm.h"

// debug
static int g_nString = 0;
static int g_mString = 0;

// global buffer
static char * g_buf = NULL;
static int g_sbuf = 0;

#define prepare_buf(sz)	\
	do {	\
		if(unlikely(g_sbuf < sz)) {	\
			g_buf = RREALLOC(r, char, g_buf, g_sbuf, sz);	\
			g_sbuf = sz;	\
		}\
	}while(0)

int rbtS_debug_count(rabbit * r)
{
	return g_nString;
}

int rbtS_debug_mem(rabbit * r)
{
	int mem = 0;
	int i;
	for(i = 0; i < r->stbl.size; ++i) {
		TString * ts = cast(TString *, r->stbl.table[i]);
		while(ts) {
			mem += rbtD_string(ts);
			ts = ts->next;
		}
	}
//	assert(mem == g_mString);
	if(mem != g_mString) {
		kLOG(r, 0, "rbtS_debug_mem() failed! mem(%d) != g_mString(%d)\n", mem, g_mString);
	}
	return g_mString + g_sbuf;
}

static inline unsigned int hash_string( const char * str, int len )
{
	unsigned int hash = len;
	unsigned int step = (len >> 5) + 1;
	for(; len > step; len -= step) {
		hash = hash ^ ((hash<<5) + (hash>>2) + str[len-1]);
	}

	return hash;
}

static inline const TString * find( rabbit * r, const char * str , int len, unsigned int hash )
{
	GCObject ** table = r->stbl.table;
	size_t size = r->stbl.size;

	if(unlikely(size == 0)) return NULL;

	size_t pos = hash % size;

	TString * ts = gco2str(table[pos]);
	while( ts ) {
		if(ts->len == len && ts->hash == hash && strncmp(gets(ts),str,len) == 0) {
			return ts;
		}
		ts = ts->next;
	}

	return NULL;
}

static inline void resize_string_table( rabbit * r ,size_t size )
{
	size_t old_size = r->stbl.size;
	r->stbl.size = size;

	r->stbl.table = RREALLOC(r, GCObject*, r->stbl.table, old_size, size);

	int i;
	for(i = old_size; i < r->stbl.size; ++i) {
		r->stbl.table[i] = NULL;
	}

	for(i = 0; i < old_size; ++i) {
		TString * ts = cast(TString *, r->stbl.table[i]);
		r->stbl.table[i] = NULL;
		TString * next;
		while( ts ) {
			next = ts->next;
			int pos = ts->hash % r->stbl.size;
			ts->next = cast(TString*, r->stbl.table[pos]);
			r->stbl.table[pos] = cast(GCObject*, ts);
			ts = next;
		}
	}
}

static void release( GCObject * obj )
{
	TString * ts = cast(TString *, obj);

	rabbit * r = ts->r;

	int pos = ts->hash % r->stbl.size;
	TString * t = cast(TString*, r->stbl.table[pos]);
	TString * pre = NULL;
	while(t) {
		if(cast(size_t,t) == cast(size_t,ts)) {
			if( cast(size_t,t) == cast(size_t,r->stbl.table[pos]) ) {
				r->stbl.table[pos] = cast(GCObject*,t->next);
			} else {
				pre->next = t->next;
			}

			{
				g_nString--;
				g_mString -= rbtD_string(ts);
			}

			r->stbl.used--;
			//			rbtM_realloc(r,ts, ts->len + 1 + sizeof(TString), 0);
			set_free_str(r, cast(TString*, ts));
			break;
		}
		pre = t;
		t = t->next;
	}
}

inline void link_ts(rabbit * r, TString * ts)
{
	if(unlikely(r->stbl.used >= r->stbl.size && r->stbl.used < (1 << 30))) {
		resize_string_table(r, max(1024, (r->stbl.used + 1) * 2));
	}

	GCObject * obj = cast(GCObject*, ts);

	rbtC_link(r, cast(GCObject*, ts), TSTRING);

	obj->gch.gc_release = release;

	size_t pos = ts->hash % r->stbl.size;

	ts->next = cast(TString*, r->stbl.table[pos]);
	r->stbl.table[pos] = cast(GCObject *, ts);
	r->stbl.used ++;
}

static inline const TString * newstr( rabbit * r, const char * str, int len, unsigned int hash )
{
	TString * ts;
	get_free_str(r, len, ts);
	memcpy(gets(ts), str, len);
	(gets(ts))[len] = '\0';
	ts->hash = hash;
	ts->len = len;

	link_ts(r, ts);

	g_nString++;
	g_mString += rbtD_string(ts);

	return ts;
}

const TString * rbtS_new( rabbit * r, const char * str )
{
	if(!str) {
		str = "";
	}
	unsigned int len = strlen(str);

	return rbtS_init_len(r,str,len);

}

const TString * rbtS_init_len( rabbit * r, const char * str, int len )
{
	unsigned int hash = hash_string( str, len );

	const TString * ts = find(r,str,len,hash);

	if(ts){
		rbtC_mark(cast(GCObject *, ts));
		return ts;
	}

	return newstr(r, str, len, hash);
}

const TString * rbtS_init_io( rabbit * r, struct i_io * io, int len )
{
	TString * ts;
	get_free_str(r, len, ts);
	int size = io->read_len(io, gets(ts), len);

	if(size != len) {
		set_free_str(r, ts);
		return NULL;
	}

	(gets(ts))[len] = 0;

	unsigned int hash = hash_string(gets(ts), len);

	ts->len = len;
	ts->hash = hash;

	const TString * old = find(r, gets(ts), len, hash);
	if(old) {
		set_free_str(r, ts);
		return old;
	}

	link_ts(r, ts);

	g_nString++;
	g_mString += rbtD_string(ts);

	return ts;
}

static inline int str2uint( const char * p, int len, int * r ) 
{
	if(len <= 0) {
		return 0;
	}

	int cur = 0;
	*r = 0;
	char c;
	while(cur < len) {
		c = p[cur];
		if(c < '0' || c > '9') {
			break;
		}

		*r = *r * 10 + c - '0';
		cur++;
	}

	return cur;
}

double rbtS_tofnum( const TString * ts )
{
	const char * p = gets(ts);

	if(ts->len <= 0) {
		return 0;
	}

	int sign = 1;
	int cur = 0;
	if(p[0] == '-') {
		sign = -1;
		cur++;
	}
	if(p[0] == '+') {
		cur++;
	}


	int i = 0;
	int pos = str2uint(p + cur, ts->len, &i);

	if( pos >= ts->len ) {
		return sign * i;
	}
	if(p[pos] == '.') {
		int j = 0;
		str2uint(p + pos + 1, ts->len - pos - 1, &j);
		double f = j;
		while(f >= 1) { f /= 10; }
		return sign * (i + f);
	}

	return sign * i;
}

const TString * rbtS_concatenate( rabbit * r, const TString * ts1, const TString * ts2 )
{
	if(ts1->len == 0) return ts2;

	if(ts2->len == 0) return ts1;

	int len = ts1->len + ts2->len;

	TString * ts;
	get_free_str(r, len, ts);//rbtM_realloc(r, NULL, 0, sizeof(TString) + len + 1);
	char* ptr = gets(ts);

	memcpy(ptr, gets(ts1), ts1->len);
	memcpy(ptr+ts1->len, gets(ts2), ts2->len);

	ptr[len] = 0;

	unsigned int hash = hash_string(ptr, len);

	ts->hash = hash;
	ts->len = len;

	const TString *ts_find = find(r, ptr, len, hash);

	if (ts_find) {
		rbtC_mark(cast(GCObject *, ts_find));
		//	rbtM_realloc(r,ts, ts->len + 1 + sizeof(TString), 0);
		set_free_str(r, ts);
		return ts_find;
	}

	link_ts(r, ts);

	g_nString++;
	g_mString += rbtD_string(ts);

	return ts;
}

const TString *rbtS_lowercase( rabbit * r, const TString * ts1 )
{
	int i, isfind = 0, lastpos = 0;
	TString *ts = NULL;
	char *ptr = NULL;
	const char * p = gets(ts1);

	for (i = 0 ; i < ts1->len ; i++) {
		if (isupper(p[i])){
			if (!isfind){
				get_free_str(r, ts1->len, ts);//rbtM_realloc(r, NULL, 0, sizeof(TString) + ts1->len + 1);
				ptr = gets(ts);
				isfind = 1;
			}
			memcpy(ptr + lastpos, p + lastpos, i - lastpos);
			ptr[i] = p[i] + 32;
			lastpos = i + 1;
		}
	}

	if (!isfind) {
		return ts1;
	}

	if (lastpos != ts1->len) {
		memcpy(ptr + lastpos, p + lastpos, ts1->len - lastpos);
	}

	ptr[ts1->len] = 0;

	unsigned int hash = hash_string(ptr, ts1->len);
	const TString *ts_find = find(r, ptr, ts1->len, hash);

	ts->hash = hash;
	ts->len = ts1->len;

	if (ts_find) {
		rbtC_mark(cast(GCObject *, ts_find));
		//	rbtM_realloc(r,ts, ts->len + 1 + sizeof(TString), 0);
		set_free_str(r, ts);
		return ts_find;
	}

	link_ts(r, ts);

	g_nString++;
	g_mString += rbtD_string(ts);

	return ts;
}

const TString * rbtS_replace( rabbit *r, const TString *string, const TString *substr, const TString *replacement)
{
	int i;
	// int pos[256];
	int * pos = RMALLOC(r, int, string->len);
	int index;
	int newstrlen;
	int sublen;
	int replacelen;

	char *tok    = NULL;
	char *newstr = NULL;
	char *oldstr = NULL;
	const char *sub;
	const char *replace;
	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if ( substr == NULL || replacement == NULL ) {
		return string;
	}

	tok = oldstr = gets(string);
	sub = gets(substr);

	replacelen = replacement->len;
	sublen = substr->len;
	index = 0;

	while ((tok = strstr(tok, sub))) {	// 定位所有 substr
		pos[index] = tok - oldstr + index * (replacelen - sublen);
		tok += sublen;
		index++;
	}
	newstrlen = string->len + index * (replacelen - sublen);
	TString * ts;
	get_free_str(r, newstrlen, ts);// (TString *)rbtM_realloc(r, NULL, 0, sizeof(TString) + newstrlen + 1);

	tok = newstr = gets(ts);
	replace = gets(replacement);

	for (i = 0; i < index; i++) {
		int currentpos = tok - newstr ;

		if (currentpos != pos[i]) {
			memcpy(tok, oldstr, pos[i] - currentpos);
			oldstr += pos[i] - currentpos;
			tok += pos[i] - currentpos;
		}

		memcpy(tok, replace, replacelen);
		oldstr += sublen;
		tok += replacelen;
	}

	if (*oldstr) {
		memcpy(tok, oldstr, strlen(oldstr));
	}

	newstr[newstrlen] = 0;

	// free pos
	RFREEVECTOR(r, pos, string->len);

	unsigned int hash = hash_string(newstr, newstrlen);

	ts->hash = hash;
	ts->len = newstrlen;

	const TString *ts_find = find(r, newstr, newstrlen, hash);

	if (ts_find) {
		rbtC_mark(cast(GCObject *, ts_find));
		//	rbtM_realloc(r,ts, ts->len + 1 + sizeof(TString), 0);
		set_free_str(r, ts);
		return ts_find;
	}

	link_ts(r, ts);

	g_nString++;
	g_mString += rbtD_string(ts);

	return ts;
}

int rbtD_string( const TString * ts )
{
	int len = sizeof(TString) + ts->len + 1;

	return len;
}


inline void vm_concatenate_str_str_fn(rabbit * r, const TString * ts_a, const TString * ts_b, TValue * out)
{
	if(unlikely(ts_a->len == 0)) {
		setstrvalue_nomark(out, ts_b);
		return;
	}
	if(unlikely(ts_b->len == 0)) {
		setstrvalue_nomark(out, ts_a);
		return;
	}
	int len = ts_a->len + ts_b->len;
	int saved_len = len;
	prepare_buf(len+1);
	memcpy(g_buf, gets(ts_a), ts_a->len);
	memcpy(g_buf + ts_a->len, gets(ts_b), ts_b->len + 1);

	// hash
	unsigned int hash = hash_string(g_buf, saved_len);

	// find
	const TString * ts = find(r, g_buf, saved_len, hash);
	if(ts) {
		setstrvalue_nomark(out, ts);
		return;
	}

	const TString * ts_c = newstr(r, g_buf, saved_len, hash);

	setstrvalue_nomark(out, ts_c);
}

// flag == 0 : str + num; flag == 1 : num + str
inline void vm_concatenate_str_num_fn(rabbit * r, const TString * ts_a, const TValue * tv_num, TValue * out, int flag)
{
// 数字最多几位
#define NUM_MAX_LEN	64
// 浮点数的话，小数几位
#define FRA_MAX_LEN	6

	static char buf[NUM_MAX_LEN];
	static int cur_pos;
	
#define save_i()	\
	if(i == 0) {	\
		buf[cur_pos--] = '0';	\
	} else {	\
		while(i > 0) {	\
			buf[cur_pos--] = '0' + i % 10;	\
			i /= 10;	\
		}	\
	}

	int num_len = 0;

	if(ttisnum(tv_num)) {
		cur_pos = NUM_MAX_LEN - 1;
		int i = numvalue(tv_num);
		int sgn = i < 0 ? 1 : 0;
		i = abs(i);
		save_i();
		if(sgn) {
			buf[cur_pos--] = '-';
		}
		num_len = NUM_MAX_LEN - cur_pos - 1;
	} else
	if(ttisfnum(tv_num)) {
		double d = numbervalue(tv_num);
		int sgn = d < 0 ? 1 : 0;
		d = abs(d);
		int i = (int)d;
		d -= i;
		int fr_len = 0;
		cur_pos = NUM_MAX_LEN - FRA_MAX_LEN - 1;
		if(d == 0) {
			// 没有小数部分
			num_len = 0;
		} else {
			buf[cur_pos++] = '.';
			while(d && fr_len < FRA_MAX_LEN) {
				d *= 10;
				fr_len++;
				int x = (int)d;
				buf[cur_pos++] = '0' + x;
				d -= x;
			}
			num_len = fr_len + 1;
		}

		cur_pos = NUM_MAX_LEN - FRA_MAX_LEN - 2;
		save_i();
		if(sgn) {
			buf[cur_pos--] = '-';
		}
		num_len += NUM_MAX_LEN - FRA_MAX_LEN - 2 - cur_pos;
	}

	char * pnum = &buf[cur_pos+1];

	int len = ts_a->len + num_len;
	int saved_len = len;
	prepare_buf(len+1);
	if(flag == 0) {
		memcpy(g_buf, gets(ts_a), ts_a->len);
		memcpy(g_buf + ts_a->len, pnum, num_len);
	} else {
		memcpy(g_buf, pnum, num_len);
		memcpy(g_buf + num_len, gets(ts_a), ts_a->len);
	}
	g_buf[len] = 0;

	// hash
	unsigned int hash = hash_string(g_buf, saved_len);

	// find
	const TString * ts = find(r, g_buf, saved_len, hash);
	if(ts) {
		setstrvalue_nomark(out, ts);
		return;
	}

	const TString * ts_c = newstr(r, g_buf, saved_len, hash);

	setstrvalue_nomark(out, ts_c);
}
