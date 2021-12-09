#ifndef packet_struct_h_
#define packet_struct_h_

#include "mblock.h"

struct Packet {
	struct MBlockIO io;

	short int count;

//	int is_encode;
//	int is_decode;
};

#endif

