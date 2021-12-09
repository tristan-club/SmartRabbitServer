#ifndef _decoder_h_
#define _decoder_h_

#include "common.h"
//#include "context.h"

/*

#define DECODE(p,len,v)	\
	{	\
		DecodeCTX * dc = decoder_init(g_rabbit_state,p,len);	\
		if(!dc) {	\
			setnilvalue(v);	\
		} else {	\
			decode_amf3(g_rabbit_state,dc,v);	\
			decoder_dealloc(g_rabbit_state,dc);	\
		}	\
	}

#define DECODE_LEN(r,p,len,v,ppos) {	\
	DecodeCTX * dc = decoder_init(r,p,len);	\
	if(dc) {	\
		decode_amf3(r,dc,v);	\
		if(ppos) {	\
			*ppos = dc->buf.pos;	\
		}	\
		decoder_dealloc(r,dc);	\
	} else {	\
		setnilvalue(v);	\
		if(ppos) {	\
			*ppos = 0;	\
		}	\
	}	\
}


int decode_amf3(rabbit * r, DecodeCTX *context, TValue * v);
*/

#endif

