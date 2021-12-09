#include "util.h"

#include "rabbit.h"

#include "math.h"

#include "string.h"
#include "table.h"
#include "gc.h"

int is_bigendian()
{
	static unsigned short int test_val = 0x00ff;
	return ((*(char*)(&test_val)) == 0);
}

int rbtUtil_assign( rabbit * r, Table * tbl, TString * key, TValue * value )
{
	const TValue * tv;

	Table * temp = tbl;

	char * p = strdup(rbtS_gets(key));

	char * prev = strtok(p, ".");
	char * next = strtok(NULL, ".");

	while(next) {
		if(prev[0] >= '0' && prev[0] <= '9') {
			int i = stof(prev, strlen(prev), NULL);
			tv = rbtH_getnum(r, temp, i);

			if(!ttistbl(tv)) {
				fprintf(stderr, "Table key(%d) is nil\n", i);

				Table * t = rbtH_init(r, 1, 1);
				settblvalue(rbtH_setnum(r, temp, i), t);

				temp = t;
			} else {
				temp = tblvalue(tv);
			}

		} else {
			tv = rbtH_getstr(r, temp, prev);

			if(!ttistbl(tv)) {
				fprintf(stderr, "Table key(%s) is nil\n", prev);

				Table * t = rbtH_init(r, 1, 1);
				settblvalue(rbtH_setstr(r, temp, prev), t);

				temp = t;
			} else {
				temp = tblvalue(tv);
			}
		}

		prev = next;
		next = strtok(NULL, ".");
	}

	if(prev[0] == '+') {

		tv = rbtH_getstr(r, temp, prev);

		int c = numbervalue(tv) + numbervalue(value);

		setfnumvalue(rbtH_setstr(r, temp, &prev[1]), c);

	} else if (prev[0] == '-') {

		tv = rbtH_getstr(r, temp, prev);

		int c = numbervalue(tv) - numbervalue(value);

		setfnumvalue(rbtH_setstr(r, temp, &prev[1]), c);

	} else {
		setvalue(rbtH_setstr(r, temp, prev), value);
	}

	free(p);

	return 0;
}

