#ifndef db_h_
#define db_h_

#include "object.h"

#define get_mysql(r) cast(MYSQL *, r->mysql)

#define rbtDB_Select( r, t, c ) rbtDB_select(r, t, c, 1, 5);

int rbtDB_init( rabbit * r, const char * host, const char * usr, const char * passwd, const char * db );

Table * rbtDB_select( rabbit * r, const char * table, const char * condition, int page, int limit );

Table * rbtDB_select_single( rabbit * r, const char * table, const char * condition );

int rbtDB_insert( rabbit * r, const char * table, Table * t , const TString ** inc, const TString ** update );

int rbtDB_update( rabbit * r, const char * table, const char * condition, Table * t, TString ** inc );

int rbtDB_delete( rabbit * r, const char * table, const char * condition );

int rbtDB_get_num( rabbit * r, const char * table, const char * condition );

#endif

