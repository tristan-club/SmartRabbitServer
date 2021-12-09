#ifndef script_load_h_
#define script_load_h_

#include "script_struct.h"

Proto *
script_load(Script * S, stream * st, Table * env);

void
script_save(Script * S, Proto * p, const char * path);

#endif
