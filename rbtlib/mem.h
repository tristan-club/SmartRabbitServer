#ifndef mem_h_
#define mem_h_

#include <stdlib.h>
#include <string.h>

#include "common.h"

#define RMALLOC(r,t,n)	(t*)rbtM_realloc( r, NULL, 0, sizeof(t) * (n) )

#define RREALLOC(r,t,op,os,ns) (t*)rbtM_realloc(r, op, sizeof(t) * (os), sizeof(t) * (ns))

#define RFREE(r,p) rbtM_realloc(r,p,sizeof(*p),0)

#define RFREEVECTOR(r, p, n) rbtM_realloc(r, p, sizeof(*p) * (n), 0);

#define ROUND(s,r) ((s+r-1)&(~(r-1)))

void * rbtM_realloc( rabbit * r, void * p, size_t osize, size_t nsize );

void rbtM_dump(rabbit * r);

#endif

