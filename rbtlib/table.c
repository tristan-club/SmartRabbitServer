#include "table.h"

#include "string.h"
#include "rabbit.h"

#include "mem.h"
#include "gc.h"
#include "math.h"

#include "table_struct.h"

#include "list.h"

// debug
static int g_nTable = 0;
static int g_mTable = 0;
static LIST_HEAD(g_TableList);

int rbtH_debug_count()
{
	int count = 0;
	struct list_head * p;
	struct Table * t;
	list_foreach(p, &g_TableList) {
		t = list_entry(p, struct Table, link);
		count++;
	}

	assert(count == g_nTable);
	
	return g_nTable;
}

int rbtH_debug_mem()
{
	int mem = 0;
	struct list_head * p;
	struct Table * t;
	list_foreach(p, &g_TableList) {
		t = list_entry(p, struct Table, link);
		mem += rbtM_table(t);
	}

	assert(mem == g_mTable);

	return g_mTable;
}

void rbtH_weak( Table * t )
{
	t->weak = 1;
}

static inline void vector_init( rabbit * r, Table * t, size_t vsize )
{
	if(vsize == 0) vsize = 1;

	int i;
	t->vector = RMALLOC(r, TValue, vsize);

	for(i = 0; i < vsize; ++i) {
		setnilvalue(&t->vector[i]);
	}

	t->vector_size = vsize;
}

static inline void table_init( rabbit * r, Table * t, size_t tsize )
{
	if(tsize <= 0) tsize = 1;

	int i;
	t->table = RMALLOC(r, struct Node, tsize);

	for(i = 0; i < tsize; ++i) {
		setnilvalue(gkey(gnode(t,i)));
		setnilvalue(gval(gnode(t,i)));
		gnode(t,i)->next = NULL;
	}

	t->table_size = tsize;
}

static inline unsigned int hash_double( double d )
{
	static char h[DOUBLE_SIZE];
	memcpy(h,&d,DOUBLE_SIZE);
	int i;
	for(i = 1; i < DOUBLE_SIZE; ++i) {
		h[0] += h[i];
	}

	return h[0];
}

static inline unsigned int hash_pointer( void * p )
{
	static char h[POINTER_SIZE];
	memcpy(h,&p,POINTER_SIZE);
	int i;
	int hash = 0;
	for(i = 0; i < POINTER_SIZE; ++i) {
		hash += h[i];
	}

	return hash;
}

static inline unsigned int hash_object( const TValue * v )
{
	switch( ttype(v) ) {
		case TNIL:
			return 0;
		case TNUMBER:
			return abs(numvalue(v));
		case TFLOAT:
			return hash_double(fnumvalue(v));
		case TSTRING:
			return rbtS_hash(strvalue(v));
		case TPOINTER:
			return hash_pointer(pvalue(v));
		default:
			return hash_pointer(cast(void*, gcvalue(v)));
	}
}

static inline struct Node * mainposition( Table * t, const TValue * tv )
{
	unsigned int hash = hash_object( tv );
	unsigned int pos = hash % t->table_size;

	return gnode(t,pos);
}

static inline void traverse( GCObject * obj )
{
	Table * t = cast(Table *, obj);

	if(t->weak) {
		return;
	}

	int i;
	for(i = 0; i < t->vector_size; ++i) {
		if(is_collectable(&t->vector[i])) {
			rbtC_mark( gcvalue(&t->vector[i]) );
		}
	}

	for(i = 0; i < t->table_size; ++i) {
		if(is_collectable( gval(gnode(t,i)) ) ) {
			rbtC_mark( gcvalue( gval(gnode(t,i)) ) );
		}
		if(is_collectable( gkey(gnode(t,i)) ) ) {
			rbtC_mark( gcvalue( gkey(gnode(t,i)) ) );
		}
	}
}

static void release( GCObject * obj )
{
	Table * t = cast(Table *, obj);

	g_mTable -= rbtM_table(t);
	g_nTable--;

	list_del(&t->link);

	RFREEVECTOR(t->r,t->vector,t->vector_size);
	RFREEVECTOR(t->r,t->table,t->table_size);
	RFREE(t->r,t);
}

Table * rbtH_init( rabbit * r, size_t vsize, size_t tsize )
{
	Table * t = RMALLOC(r,Table, 1);

	rbtC_link(r, cast(GCObject*, t), TTABLE);

	vector_init( r, t, vsize );

	table_init( r, t, tsize );

	t->last_free_pos = t->table_size - 1;

	t->weak = 0;

	t->gc_traverse = traverse;

	t->gc_release = release;

	g_nTable++;
	g_mTable += rbtM_table(t);

	list_init(&t->link);
	list_insert(&g_TableList, &t->link);

	return t;
}

Table * rbtH_init_no_link(rabbit * r, size_t vsize, size_t tsize )
{
	Table * t = RMALLOC(r, Table, 1);

	vector_init(r, t, vsize);

	table_init(r, t, tsize);

	t->last_free_pos = t->table_size - 1;

	t->weak = 0;

	t->gc_traverse = NULL;
	t->gc_release = NULL;

	r->obj++;

	g_nTable++;
	g_mTable += rbtM_table(t);

	list_insert(&g_TableList, &t->link);

	return t;
}

#define get_str_quick(result, r, t, key)	\
	do {	\
		result = NULL;	\
		unsigned int hash = rbtS_hash(key);	\
		struct Node * n = gnode(t, hash % t->table_size);	\
		while(n) {	\
			if(ttisstr(gkey(n)) && cast(size_t, strvalue(gkey(n))) == cast(size_t, key)) {	\
				result = gval(n);	\
				break;	\
			}	\
			n = n->next;	\
		}	\
	}while(0)

static inline const TValue * getstr( rabbit * r, Table * t, const TString * key )
{
	unsigned int hash = rbtS_hash(key);
	struct Node * n = gnode(t, hash % t->table_size);

	while(n) {
		if(ttisstr(gkey(n)) && cast(size_t, strvalue(gkey(n))) == cast(size_t, key)) {
			return gval(n);
		}
		n = n->next;
	}

	return NULL;
}

static inline const TValue * getcstr( rabbit * r, Table * t, const char * key )
{
	const TString * ts = rbtS_new( r, key );

	return getstr(r, t, ts);
}


inline const TValue * rbtH_getstr( rabbit * r, Table * t, const char * key )
{
	const TValue * tv = getcstr( r, t, key );
	if(tv) {
		return tv;
	}
	return rbtT_Nil;
}

inline const TValue * rbtH_gettstr( rabbit * r, Table * t, const TString * key )
{
	const TValue * tv = getstr( r, t, key );
	if(tv) {
		return tv;
	}
	return rbtT_Nil;
}

static inline const TValue * getnum( rabbit * r, Table * t, int key )
{
	if( key >= 0 && key < t->vector_size ) {
		return &t->vector[key];
	}

	int hash = abs(key);
	struct Node * n = gnode(t,hash % t->table_size);

	while( n ) {
		if(ttisnum(gkey(n)) && numvalue(gkey(n)) == key ) {
			return gval(n);
		}
		/*
		if(ttisnil(gkey(n))) {
			break;
		}*/
		n = n->next;
	}

	return NULL;
}

inline const TValue * rbtH_getnum( rabbit * r, Table * t, int key )
{
	const TValue * tv = getnum( r, t, key );
	if(tv) {
		return tv;
	}

	return rbtT_Nil;
}

static inline const TValue * get( rabbit * r, Table * t, const TValue * key )
{
	switch( ttype(key) ) {
		case TSTRING:
			return getstr(r,t,strvalue(key));
		case TNUMBER:
			return getnum(r,t,numvalue(key));
		case TFLOAT:
			{
				double fnum = fnumvalue(key);
				if((double)((int)fnum) == fnum) {
					return getnum(r, t, (int)fnum);
				}
			}
			break;

		default:
			break;
	}

	struct Node * n = mainposition(t, key);
	while( n ) {
		if(rbtO_rawequ(gkey(n), key)) {
			return gval(n);
		}

		n = n->next;
	}

	return NULL;
}

inline const TValue * rbtH_get( rabbit * r, Table * t, const TValue * key )
{
	const TValue * tv = get( r, t, key );

	if(tv) {
		return tv;
	}

	return rbtT_Nil;
}

static inline void caculnum( unsigned int i, int * num )
{
	int p = rbtM_log2(i);
	if(p > 30) return;	// 如此大的数,还是不计算在内的好

	num[p+1]++;
}

static inline int collect_info( rabbit * r, Table * t, int * num )
{
	int i,totalnum = 0;
	for(i = 0; i < t->vector_size; ++i) {
		if(!ttisnil(&t->vector[i])) {
			caculnum(i,num);
			totalnum++;
		}
	}
	struct Node * n;
	for(i = 0; i < t->table_size; ++i) {
		n = gnode(t,i);
		if(!ttisnil(gval(n))) {
			totalnum++;
			if(ttisnum(gkey(n))) {
				caculnum(numvalue(gkey(n)),num);
			}
		}
	}

	return totalnum;
}

static inline int compute_size( rabbit * r, Table * t, int * numsize, int * optsize )
{
	int num[32],totalnum;
	memset(num,0,sizeof(num));
	totalnum = collect_info( r, t, num );

	int i, n = 0, p = 0,c = 0;
	for(i = 0; i < 31; ++i) {
		n += num[i];
		if(n >= rbtM_pow2(i) / 2) {
			p = i;
			c = n;
		}
	}

	*numsize = c;
	*optsize = rbtM_pow2(p);

	return totalnum;
}

static inline void resize( rabbit * r, Table * t )
{
	int old_mem = rbtM_table(t);

	int totalsize,numsize,optsize;
	totalsize = compute_size(r, t, &numsize, &optsize);
	int tablesize = totalsize - numsize;

	size_t old_vector_size = t->vector_size;
	TValue * old_vector = t->vector;
	vector_init(r,t,optsize);

	size_t old_table_size = t->table_size;
	struct Node * old_table = t->table;
	tablesize = (tablesize + 1) * 2;	// 装载因子为2，每次查找平均不超过2次
	table_init(r, t, tablesize);
	t->last_free_pos = t->table_size - 1;

	int i;
	for(i = 0; i < old_vector_size; ++i) {
		if(!ttisnil(&old_vector[i])) {
			setvalue(rbtH_setnum(r,t,i),&old_vector[i]);
		}
	}

	for(i = 0; i < old_table_size; ++i) {
		struct Node * n = &old_table[i];
		if(!ttisnil(gval(n))) {
			setvalue(rbtH_set(r,t,gkey(n)),gval(n));
		}
	}

	RFREEVECTOR(r, old_vector, old_vector_size);
	RFREEVECTOR(r, old_table, old_table_size);

	g_mTable += rbtM_table(t) - old_mem;
}

static inline struct Node * get_free_pos( Table * t ) 
{
	while(t->last_free_pos >= 0) {
		struct Node * n = gnode(t,t->last_free_pos);
		if(ttisnil(gval(n)) && ttisnil(gkey(n))) {
			// 必须是 key 和 val 都是 nil, 才是一个空闲位置
			// 如果只是 val 为 nil, 那么这个node里还有 next 信息不能丢掉, 所以不能当做空闲位置
			return n;
		}
		t->last_free_pos --;
	}

	return NULL;
}

static inline TValue * newkey( rabbit * r, Table * t, const TValue * key )
{
	struct Node * fn = get_free_pos(t);
	if(!fn) {
		resize( r, t );
		return rbtH_set(r, t, key);
	}

	struct Node * mp = mainposition(t, key);

	if(!ttisnil(gval(mp))) {
		struct Node * othern = mainposition(t, gkey(mp));

		if(othern != mp) {
			// not at its main position
			*fn = *mp;
			while(othern->next && othern->next != mp) othern = othern->next;
			othern->next = fn;
			mp->next = NULL;
		} else {
			fn->next = mp->next;
			mp->next = fn;
			mp = fn;
		}
	}

	setnilvalue(gval(mp));
	setvalue(gkey(mp),key);

	return gval(mp);
}

inline TValue * rbtHVM_newkey(rabbit * r, Table * t, const TValue * key)
{
	return newkey(r, t, key);
}

inline TValue * rbtH_setnum( rabbit * r, Table * t, int key )
{
	const TValue * tv = getnum(r,t,key);
	if(tv) {
		return cast(TValue *, tv);
	}

	TValue v;
	setnumvalue(&v,key);

	return newkey(r, t, &v);
}

inline TValue * rbtH_setnextnum( rabbit * r, Table * t )
{
	int i;
	for(i = 0; i < t->vector_size; ++i) {
		if(ttisnil(&t->vector[i])) {
			break;
		}
	}

	if(i >= t->vector_size ) {
		t->vector = RREALLOC(r, TValue, t->vector, t->vector_size, t->vector_size + 2);
		setnilvalue(&t->vector[t->vector_size]);
		setnilvalue(&t->vector[t->vector_size+1]);
		t->vector_size += 2;

		rbtH_rmnum(r, t, i);

		g_mTable += 2 * sizeof(TValue);
	}

	return &t->vector[i];
}

inline TValue * rbtH_setstr( rabbit * r, Table * t, const char * key )
{
	const TString * ts = rbtS_new( r, key );

	const TValue * tv = getstr(r,t,ts);
	if(tv) {
		return cast(TValue *, tv);
	}

	TValue v;
	setstrvalue(&v, ts);

	return newkey(r,t,&v);
}

inline TValue * rbtH_settstr( rabbit * r, Table * t, const TString * key )
{
	const TValue * tv = getstr(r,t,key);
	if(tv) {
		return cast(TValue *, tv);
	}

	TValue v;
	setstrvalue(&v, key);

	return newkey(r,t,&v);
}

inline TValue * rbtH_set( rabbit * r, Table * t, const TValue * key )
{
	if(ttisfnum(key)) {
		double d_key = fnumvalue(key);

		if((double)((int)d_key) == d_key) {
			return rbtH_setnum(r, t, (int)d_key);
		}
	}

	const TValue * tv = get(r, t, key);
	if(tv) {
		return cast(TValue *, tv);
	}

	return newkey(r,t,key);
}

int rbtH_next( rabbit * r, Table * t, int idx, TValue * k, TValue * v )
{
	if(idx < 0) {
		idx = -1;
	} 

	TValue * tv;
	for(idx++;idx < t->vector_size; ++idx) {
		tv = &t->vector[idx];
		if(!ttisnil(tv)) {
			if(k) setnumvalue(k,idx);
			if(v) *v = *tv;
			return idx;
		}
	}
	idx -= t->vector_size;
	for(; idx < t->table_size; ++idx) {
		struct Node * n = gnode(t,idx);
		if(!ttisnil(gval(n))) {
			if(k) *k = *gkey(n);
			if(v) *v = *gval(n);
			return idx + t->vector_size;
		}
	}

	setnilvalue(v);
	return -1;
}

int rbtH_count( rabbit * r, Table * t )
{
	int i,count = 0;
	TValue * tv;
	for(i = 0; i < t->vector_size; ++i) {
		tv = &t->vector[i];
		if(!ttisnil(tv)) {
			count ++;
		}
	}
	for(i = 0; i < t->table_size; ++i) {
		struct Node * n = gnode(t,i);
		if(!ttisnil(gval(n))) {
			count ++;
		}
	}

	return count;
}

int rbtH_empty( rabbit * r, Table * t )
{
	int i;
	TValue * tv;
	for(i = 0; i < t->vector_size; ++i) {
		tv = &t->vector[i];
		if(!ttisnil(tv)) {
			return 0;
		}
	}

	for(i = 0;i < t->table_size; ++i) {
		struct Node * n = gnode(t,i);
		if(!ttisnil(gval(n))) {
			return 0;
		}
	}

	return 1;
}

int rbtH_countnum( rabbit * r, Table * t )
{
	int i,count = 0;
	TValue * tv;
	for(i = 0; i < t->vector_size; ++i) {
		tv = &t->vector[i];
		if(!ttisnil(tv)) {
			count ++;
		}
	}

	return count;
}

int rbtH_countstr( rabbit * r, Table * t )
{
	int i,count = 0;
	for(i = 0; i < t->table_size; ++i) {
		struct Node * n = gnode(t,i);
		if(ttisstr(gkey(n)) && !ttisnil(gval(n))) {
			count++;
		}
	}

	return count;
}

void rbtH_clean(rabbit * r, Table * t)
{
	int i;
	for(i = 0; i < t->vector_size; ++i) {
		setnilvalue(&t->vector[i]);
	}
	for(i = 0; i < t->table_size; ++i) {
		struct Node * n = gnode(t, i);
		setnilvalue(gkey(n));
		setnilvalue(gval(n));
		n->next = NULL;
	}
	t->last_free_pos = t->table_size - 1;
}

static int _value_dump( const TValue * tv )
{
	int ntab = 1;
	if(ttisnum(tv)) {
		fprintf(stderr, "%d", numvalue(tv));
	}
	if(ttisfnum(tv)) {
		fprintf(stderr, "%f", fnumvalue(tv));
	}
	if(ttisstr(tv)) {
		fprintf(stderr, "\"%s\"", rbtS_gets(strvalue(tv)));

		ntab = (rbtS_len(strvalue(tv)) + 7) / 8;
	}
	if(ttisbool(tv)) {
		if(ttistrue(tv)) {
			fprintf(stderr, "True");
		} else {
			fprintf(stderr, "False");
		}
	}
	if(ttisclosure(tv)) {
		fprintf(stderr, "  <CLOSURE(%p)>", closurevalue(tv));
	}
	if(ttisp(tv)) {
		fprintf(stderr, "%p", (void *)pvalue(tv));
	}

	return ntab;
}

static int _table_dump( rabbit * r, Table * t, int depth )
{
	int i;
	if(!t || depth >= 255) return 0;

	char ntab[256];
	memset(ntab, 0, sizeof(ntab));
	for(i = 0; i < depth; ++i) {
		ntab[i] = '\t';
	}

	fprintf(stderr, "%s{\n",ntab);		//  begin with : {
	ntab[depth] = '\t';
	for(i = 0; i < t->vector_size; ++i) {
		TValue * tv = &t->vector[i];
		if(!ttisnil(tv)) {
			fprintf(stderr, "%s[%d]\t=>", ntab, i);
			if(ttistbl(tv)) {
				fprintf(stderr, "array[%d]\n", rbtH_count(r, tblvalue(tv)));
				_table_dump(r, tblvalue(tv), depth + 2);
			} else {
				_value_dump( tv );
				fprintf(stderr, "\n");
			}
		}
	}

	for(i = 0; i < t->table_size; ++i) {
		int d = depth ;//+ 1;
		struct Node * node = gnode(t,i);
		if(!ttisnil(gval(node))) {
			fprintf(stderr, "%s", ntab);
			fprintf(stderr, "[");
			d += _value_dump( gkey(node) );

			if(ttisstr(gkey(node))) {
				TString * strkey = strvalue(gkey(node));
				if(strcmp(rbtS_gets(strkey), "Root") == 0) {
					fprintf(stderr, "]\n");
					continue;
				}
			}

			fprintf(stderr, "](%d)", i);
			fprintf(stderr, "\t=>");
			if(ttistbl(gval(node))) {
				fprintf(stderr, "array[%d]\n", rbtH_count(r, tblvalue(gval(node))));
				_table_dump(r, tblvalue(gval(node)), d + 2);
			} else {
				_value_dump( gval(node) );
				fprintf(stderr, "\n");
			}
		}
	}

	ntab[depth] = '\0';
	fprintf(stderr, "%s}\n", ntab);

	return 0;
}

int rbtD_table( rabbit * r, Table * t )
{
	fprintf(stderr, "array[%d]\n", rbtH_count(r, t));
	_table_dump(r, t, 0);
	return 0;
}

static int _value_log_dump(const TValue * tv, char * outstr)
{
	int ntab = 1;
	if(ttisnum(tv)) {
		sprintf(outstr + strlen(outstr), "%d\n", numvalue(tv));
	}
	if(ttisfnum(tv)) {
		sprintf(outstr + strlen(outstr), "%f\n", fnumvalue(tv));
	}
	if(ttisstr(tv)) {
		sprintf(outstr + strlen(outstr), "\"%s\"\n", rbtS_gets(strvalue(tv)));

		ntab = (rbtS_len(strvalue(tv)) + 7) / 8;
	}
	if(ttisbool(tv)) {
		if(ttistrue(tv)) {
			sprintf(outstr + strlen(outstr), "True\n");
		} else {
			sprintf(outstr + strlen(outstr), "False\n");
		}
	}
	if(ttisclosure(tv)) {
		sprintf(outstr + strlen(outstr), "  <CLOSURE(%p)>\n", closurevalue(tv));
	}

	return ntab;
}

static int _table_log_dump( rabbit * r, Table * t, int pid, int depth )
{
	int i;
	if(!t || depth >= 255) return 0;

	char ntab[256];
	char outstr[1000];
	memset(ntab, 0, sizeof(ntab));
	for(i = 0; i < depth; ++i) {
		ntab[i] = '\t';
	}

	kLOG(r, pid, "%s{\n",ntab);		//  begin with : {
	ntab[depth] = '\t';
	for(i = 0; i < t->vector_size; ++i) {
		TValue * tv = &t->vector[i];
		if(!ttisnil(tv)) {
			sprintf(outstr, "%s[%d]\t=>\n", ntab, i);
			if(ttistbl(tv)) {
				sprintf(outstr + strlen(outstr), "array[%d]\n", rbtH_count(r, tblvalue(tv)));
				kLOG(r, pid, "%s\n", outstr);
				_table_log_dump(r, tblvalue(tv), pid, depth + 2);
			} else {
				_value_log_dump(tv, outstr);
				kLOG(r, pid, "%s\n", outstr);
			}
		}
	}

	for(i = 0; i < t->table_size; ++i) {
		int d = depth ;//+ 1;
		struct Node * node = gnode(t,i);
		if(!ttisnil(gval(node))) {
			sprintf(outstr, "%s[\n", ntab);
			d += _value_log_dump(gkey(node), outstr);

			if(ttisstr(gkey(node))) {
				TString * strkey = strvalue(gkey(node));
				if(strcmp(rbtS_gets(strkey), "Root") == 0) {
					sprintf(outstr + strlen(outstr), "]\n");
					kLOG(r, pid, "%s\n", outstr);
					continue;
				}
			}

			sprintf(outstr + strlen(outstr), "](%d)\t=>\n", i);
			if(ttistbl(gval(node))) {
				sprintf(outstr + strlen(outstr), "array[%d]", rbtH_count(r, tblvalue(gval(node))));
				kLOG(r, pid, "%s\n", outstr);
				_table_log_dump(r, tblvalue(gval(node)), pid, d + 2);
			} else {
				_value_log_dump(gval(node), outstr);
				kLOG(r, pid, "%s\n", outstr);
			}
		}
	}

	ntab[depth] = '\0';
	kLOG(r, pid, "%s}\n", ntab);

	return 0;
}

int rbtLog_table( rabbit * r, Table * t, int pid )
{
	kLOG(r, pid, "array[%d]\n", rbtH_count(r, t));
	_table_log_dump(r, t, pid, 0);
	return 0;
}

int rbtM_table( Table * t )
{
	int m = sizeof(Table);
	m += t->vector_size * sizeof(TValue);
	m += t->table_size * sizeof(struct Node);

	return m;
}
