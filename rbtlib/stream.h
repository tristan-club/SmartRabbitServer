#ifndef _stream_h_
#define _stream_h_

#include "object.h"

struct stream {
	CommonHeader;

	const char * p;
	int size;

	const TString * filename;
};

stream * stream_open(rabbit * r, const char * file);

#endif

