#ifndef svr_connection_h_
#define svr_connection_h_

#include "server.h"

#define CONN_ADMIN	1
#define CONN_SERV	2
#define CONN_GATE	3

int rbtF_conn_broken( rabbit * r, Connection * c );

#endif

