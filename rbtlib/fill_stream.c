#include "fill_stream.h"

void fill_int( char * p, int i )
{
	p[0] = i & 0xff;
	p[1] = (i >> 8 ) & 0xff;
	p[2] = (i >> 16 ) & 0xff;
	p[3] = (i >> 24 ) & 0xff;
}

void fill_short( unsigned char * p, short int i )
{
	p[0] = i & 0xff;
	p[1] = (i >> 8) & 0xff;
}
