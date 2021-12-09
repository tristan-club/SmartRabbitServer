#ifndef script_misc_h_
#define script_misc_h_

#include "script.h"

struct misc_debug {
	int nsleep_param;
	int msleep_param;
};

void rbtScript_sleep(Script * S, int msec);
void rbtScript_sleep_checktimeout(Script * S);

void rbtScript_traverse(rabbit * r);

struct misc_debug * misc_get_debug();

#endif
