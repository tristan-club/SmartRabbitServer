#ifndef _amf_h_
#define _amf_h_

#include "common.h"
#include "object.h"
#include "io.h"

int rbtAMF_decode(rabbit * r, struct i_io * in, TValue * out);

int rbtAMF_encode(rabbit * r, const TValue * in, struct i_io * out);

#endif

