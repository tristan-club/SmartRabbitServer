#ifndef amf_common_h_ 
#define amf_common_h_ 

#include "object.h"
#include "rawbuffer.h"

#include "rabbit.h"

// Valid AMF3 integer range
#define MIN_INT -268435457
#define MAX_INT 268435456

// Reference bit
#define REFERENCE_BIT           0x01

// Empty string
#define EMPTY_STRING_TYPE       0x01

// Object Headers
#define STATIC                  0x03
#define DYNAMIC                 0x0B
#define EXTERNALIZABLE          0x07

// Type markers
#define UNDEFINED_TYPE          0x00
#define NULL_TYPE               0x01
#define FALSE_TYPE              0x02
#define TRUE_TYPE               0x03
#define INT_TYPE                0x04
#define DOUBLE_TYPE             0x05
#define STRING_TYPE             0x06
#define XML_DOC_TYPE            0x07
#define DATE_TYPE               0x08
#define ARRAY_TYPE              0x09
#define OBJECT_TYPE             0x0A
#define XML_TYPE                0x0B
#define BYTE_ARRAY_TYPE         0x0C


typedef struct {
    TValue * list;
    int len;
    int pos;
}idxbuffer;

#define _idx_buf_pre_allocate_len_ 32

extern int			idx_map		(rabbit * r, idxbuffer* self,TValue* obj);
extern TValue*			idx_ret_index	(rabbit * r, idxbuffer* self,int key);

extern int			idx_ret_value	(rabbit * r, idxbuffer* self,TValue* obj);

typedef struct Traits {
	TString ** list;
	int nsealed;
	int size;

	int dynamic;
}Traits;

typedef struct TraitsList {
	Traits * list;
	int used;
	int size;
}TraitsList;

typedef struct DecodeCTX {
	struct i_io * io;
	idxbuffer obj_refs;		// idxbuffer for objects
	idxbuffer string_refs;		// idxbuffer for strings
	TraitsList traits_refs;		// traits reference
	jmp_buf err_jmp;
}DecodeCTX;

extern void		decoder_init(rabbit * r, DecodeCTX * self);
extern void 		decoder_dealloc(rabbit * r, DecodeCTX *self);

typedef struct EncoderObj {
	struct i_io * io;
	idxbuffer obj_refs; 	// idxbuffer for objects
	idxbuffer string_refs; 	// idxbuffer for strings
	idxbuffer class_refs; 	// idxbuffer for ClassDefs
}EncoderObj;

extern EncoderObj*	encoder_init			(rabbit * r);
extern void 		encoder_dealloc			(rabbit * r, EncoderObj *self);

extern struct EncoderObj en_obj;

#endif

