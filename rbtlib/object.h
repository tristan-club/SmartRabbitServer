#ifndef object_h_
#define object_h_

#include "common.h"
#include "list.h"

//#define CommonHeader	GCObject * gclist; E_TYPE tt; int mark; void(*gc_traverse)(GCObject *); void(*gc_release)(GCObject * ); rabbit * r; char * debug_name
#define CommonHeader	struct list_head gclist; E_TYPE tt; int mark; void(*gc_traverse)(GCObject *); void(*gc_release)(GCObject * ); rabbit * r; char * debug_name

typedef struct GCHeader {
	CommonHeader;
}GCHeader;

union GCObject {
	GCHeader gch;
};


/*
 *	TValue
 */
typedef union Value {
	GCObject * gc;
	int i;
	double d;
	void * p;
	int b;
	double t;
}Value;

typedef struct TValue {
	Value value;
	E_TYPE tt;
}TValue;

#define rbtT_Nil &rbtT_nilvalue
extern const TValue rbtT_nilvalue;


#define ttisfalse(tv) (ttisnil(tv) || (ttisbool(tv) && bvalue(tv) == 0))
#define ttistrue(tv) (!ttisfalse(tv))

#define ttype(tv) (tv)->tt
#define ttisnil(tv) (ttype(tv) == TNIL)
#define ttisnum(tv) (ttype(tv) == TNUMBER)
#define ttisfnum(tv) (ttype(tv) == TFLOAT)
#define ttisnumber(tv) (ttisnum(tv) || ttisfnum(tv))
#define ttisp(tv) (ttype(tv) == TPOINTER)
#define ttisbool(tv) (ttype(tv) == TBOOL)
#define ttisstr(tv) (ttype(tv) == TSTRING)
#define ttistbl(tv) (ttype(tv) == TTABLE)
#define ttisconn(tv) (ttype(tv) == TCONNECTION)
#define ttispkt(tv) (ttype(tv) == TPACKET)
#define ttisqueue(tv) (ttype(tv) == TQUEUE)
#define ttislru(tv) (ttype(tv) == TLRU)
#define ttisbuffer(tv) (ttype(tv) == TBUFFER)
#define ttiseventhandler(tv) (ttype(tv) == TEVENTHANDLER)
#define ttisstream(tv) (ttype(tv) == TSTREAM)
#define ttisuserdata(tv) (ttype(tv) == TUSERDATA)

#define ttisclosure(tv) (ttype(tv) == TCLOSURE)

#define numvalue(o) (ttisnum(o) ? (o)->value.i : 0)
#define pvalue(o) ((o)->value.p)
#define bvalue(o) ((o)->value.b)
#define boolvalue(o) bvalue(o)
#define datevalue(o) ((o)->value.t)
#define fnumvalue(o) ((o)->value.d)
#define numbervalue(o) (ttisfnum(o) ? fnumvalue(o) : numvalue(o))
#define gcvalue(o) ((o)->value.gc)
#define strvalue(o) (gco2str((o)->value.gc))
#define cstrvalue(o) gets(strvalue(o))
#define tblvalue(o) (gco2tbl((o)->value.gc))
#define connvalue(o) (gco2conn((o)->value.gc))
#define pktvalue(o) (gco2pkt((o)->value.gc))
#define queuevalue(o) (gco2queue((o)->value.gc))
#define lruvalue(o) (gco2lru((o)->value.gc))
#define buffervalue(o) (gco2buffer((o)->value.gc))
#define netmgrvalue(o) (gco2netmgr((o)->value.gc))
#define streamvalue(o) (gco2stream((o)->value.gc))
#define udvalue(o) (gco2ud((o)->value.gc))

#define closurevalue(o) (gco2cl((o)->value.gc))
#define scriptvalue(o) (gco2script((o)->value.gc))
#define protovalue(o) (gco2proto((o)->value.gc))


#define setgcvalue(v,o)	\
	{ TValue * i_v = (v); GCObject * i_g = cast(GCObject*, o); ttype(i_v) = i_g->gch.tt; i_v->value.gc = i_g; rbtC_mark(i_g); }

#define setnilvalue(o) (ttype(o) = TNIL)

#define setpvalue(o,pnt) 	\
	{ TValue * i_o = (o); ttype(i_o) = TPOINTER; i_o->value.p =  pnt; }

#define setstrvalue(o,ts)	\
	{ TValue * i_o = (o); ttype(i_o) = TSTRING; i_o->value.gc = cast(GCObject *, ts); rbtC_mark(cast(GCObject *, ts)); }

#define setstrvalue_nomark(o,ts)	\
	setstrvalue(o, ts)
//	{ TValue * i_o = (o); ttype(i_o) = TSTRING; i_o->value.gc = cast(GCObject *, ts); }

#define setnumvalue(o,n)	\
	{ TValue * i_o = (o); ttype(i_o) = TNUMBER;i_o->value.i = n; }

#define setfloatvalue(o,f)	\
	{ TValue * i_o = (o); ttype(i_o) = TFLOAT; i_o->value.d = f; }

#define setfnumvalue(o,f)	setfloatvalue(o,f)

#define setboolvalue(o,_b)	\
	{ TValue * i_o = (o); ttype(i_o) = TBOOL; i_o->value.b = _b; }

#define setdatevalue(o,t)	\
	{ TValue * i_o = (o); ttype(i_o) = TDATE; i_o->value.t = t; }

#define settblvalue(o,t)	\
	{ TValue * i_o = (o); ttype(i_o) = TTABLE; i_o->value.gc = cast(GCObject *, t); rbtC_mark(cast(GCObject *, t)); }

#define setpoolvalue(o,p)	\
	{ TValue * i_o = (o); ttype(i_o) = TPOOL; i_o->value.gc = cast(GCObject *, p); rbtC_mark(cast(GCObject *, p)); }

#define setconnvalue(o,c)	\
	{ TValue * i_o = (o); ttype(i_o) = TCONNECTION; i_o->value.gc = cast(GCObject *, c); rbtC_mark(cast(GCObject *, c)); }

#define setbuffervalue(o,b)	\
	{ TValue * i_o = (o); ttype(i_o) = TBUFFER; i_o->value.gc = cast(GCObject *, b); rbtC_mark(cast(GCObject *, b)); }

#define setpktvalue(o,p)	\
	{ TValue * i_o = (o); ttype(i_o) = TPACKET; i_o->value.gc = cast(GCObject *, p); rbtC_mark(cast(GCObject *, p)); }

#define setqueuevalue(o,q)	\
	{ TValue * i_o = (o); ttype(i_o) = TQUEUE; i_o->value.gc = cast(GCObject *, q); rbtC_mark(cast(GCObject *, q)); }

#define setlruvalue(o,l)	\
	{ TValue * i_o = (o); ttype(i_o) = TLRU; i_o->value.gc = cast(GCObject *, l); rbtC_mark(cast(GCObject *, l)); }

#define setstreamvalue(o,f)	\
	{ TValue * i_o = (o); ttype(i_o) = TSTREAM; i_o->value.gc = cast(GCObject *, f); rbtC_mark(cast(GCObject *, f)); }

#define setclosurevalue(o,cl)	\
	{ TValue * i_o = (o); ttype(i_o) = TCLOSURE; i_o->value.gc = cast(GCObject *, cl); rbtC_mark(cast(GCObject *, cl)); }

#define setscriptvalue(o,s)	\
	{ TValue * i_o = (o); ttype(i_o) = TSCRIPT; i_o->value.gc = cast(GCObject *, s); rbtC_mark(cast(GCObject *, s)); }

#define setprotovalue(o, p)	\
	{ TValue * i_o = (o); ttype(i_o) = TPROTO; i_o->value.gc = cast(GCObject*, p); rbtC_mark(cast(GCObject *, p)); }

#define setvalue(o1,o2)		\
	{ TValue * i_o1 = (o1); const TValue * i_o2 = (o2); ttype(i_o1) = ttype(i_o2); i_o1->value = i_o2->value; if(is_collectable(i_o1)){ rbtC_mark(gcvalue(i_o1));} }

#define setvalue_nomark(o1,o2) 	\
	setvalue(o1, o2)
//	{ TValue * i_o1 = (o1); const TValue * i_o2 = (o2); ttype(i_o1) = ttype(i_o2); i_o1->value = i_o2->value; }


inline int rbtO_rawequ(const TValue * o1, const TValue * o2);

inline double rbtO_rawcmp(const TValue * o1, const TValue * o2);

inline const TString * rbtO_rawToString( rabbit * r, const TValue * o );

void debug_tvalue_dump(const TValue * tv);

/*
 *	Buffer
 */
/*struct Buffer {		// 12 bytes + 12 bytes = 24 bytes
	size_t used;
	size_t size;

	size_t pos;	// for read/write 

	char * p;
};*/

/*
 *	UserData
 */
struct UserData {
	CommonHeader;
};

#endif

