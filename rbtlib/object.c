#include "object.h"

#include "string.h"
#include "table.h"
#include "table_struct.h"
#include "math.h"
#include "rabbit.h"

const TValue rbtT_nilvalue = {{0},TNIL};

inline int rbtO_rawequ(const TValue * o1, const TValue * o2)
{
	if(ttisnil(o1) || ttisnil(o2)) {
		return 0;
	}

	if(ttisnumber(o1) && ttisnumber(o2)) {
		return numbervalue(o2) == numbervalue(o1);
	}

	if(ttype(o1) != ttype(o2)) {
		return 0;
	}

	switch(ttype(o1)) {

		case TBOOL:
			return bvalue(o1) == bvalue(o2);

		case TDATE:
			return datevalue(o1) == datevalue(o2);

		case TPOINTER:
			return pvalue(o1) == pvalue(o2);

		default:
			break;
	}

	return gcvalue(o1) == gcvalue(o2);
}

static inline int can_cvt_to_num(const TValue * o) 
{
	if(ttisnum(o) || ttisbool(o) || ttisfnum(o)) {
		return 1;
	}

	if(ttisstr(o)) {
		const char * p = rbtS_gets(strvalue(o));
		while(*p) {
			if(*p < '0' || *p > '9') {
				return 0;
			}
			p++;
		}

		return 1;
	}

	return 0;
}

static inline double cvt_to_num(const TValue * o)
{
	if(ttisstr(o)) {
		return rbtS_tofnum(strvalue(o));
	}
	if(ttisnum(o)) {
		return numvalue(o);
	}
	if(ttisfnum(o)) {
		return fnumvalue(o);
	}
	if(ttisbool(o)) {
		return bvalue(o);
	}

	return -1;
}


inline double rbtO_rawcmp(const TValue * o1, const TValue * o2)
{
	if(likely(ttype(o1) == ttype(o2))) {
		if(ttisfnum(o1)) {
			return fnumvalue(o1) - fnumvalue(o2);
		}
		if(ttisnum(o1)) {
			return numvalue(o1) - numvalue(o2);
		}
		if(ttisbool(o1)) {
			return (bvalue(o1) - bvalue(o2));
		}
		if(ttisstr(o1)) {
			if(strvalue(o1) == strvalue(o2)) {
				return 0;
			}
			return strcmp(rbtS_gets(strvalue(o1)), rbtS_gets(strvalue(o2)));
		}
		return cast(size_t,gcvalue(o1)) - cast(size_t,gcvalue(o2));
	}
	
	if(!can_cvt_to_num(o1) || !can_cvt_to_num(o2)) {
		return -1;
	}

	double d1,d2;
	d1 = cvt_to_num(o1);
	d2 = cvt_to_num(o2);

	return d1 - d2;
}

inline const TString * rbtO_rawToString( rabbit * r, const TValue * o )
{
	if(ttisstr(o)) {
		return strvalue(o);
	}
	if(ttisnil(o)) {
		return rbtS_new(r, "null");
	}
	if(ttistbl(o)) {
		return rbtS_new(r, "Table");
	}
	if(ttisnum(o)) {
		return rbtS_new(r,itos(numvalue(o)));
	}
	if(ttisfnum(o)) {
		return rbtS_new(r, ftos(fnumvalue(o)));
	}
	if(ttisbool(o)) {
		if(bvalue(o) != 0) {
			return rbtS_new(r, "true");
		} else {
			return rbtS_new(r, "false");
		}
	}

	return rbtS_new(r, "Object");
}

void debug_tvalue_dump(const TValue * tv) {
	if(tv) {
		switch(ttype(tv)) {
			case TNIL:
				fprintf(stderr,"NIL\n");
				break;
			case TNUMBER:
				fprintf(stderr,"%d\n",numvalue(tv));
				break;
			case TFLOAT:
				fprintf(stderr,"%f\n",fnumvalue(tv));
				break;
			case TBOOL:
				fprintf(stderr,"TBOOL(%d)\n",bvalue(tv));
				break;
			case TDATE:
				fprintf(stderr,"TDATE(%f)\n.",datevalue(tv));
				break;
			case TSTRING:
				fprintf(stderr,"\"%s\"\n",rbtS_gets(strvalue(tv)));
				break;
			case TTABLE:
				rbtD_table(NULL, tblvalue(tv));
				break;
			default:
				fprintf(stderr,"TYPE: OTHER(%p).\n",gcvalue(tv));
		}
	}
}
