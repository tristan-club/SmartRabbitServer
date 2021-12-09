#ifndef util_h_
#define util_h_

#include "object.h"

int is_bigendian();

int rbtUtil_assign( rabbit * r, Table * t, TString * key, TValue * value );

#endif

