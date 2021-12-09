#ifndef md5_2_h_
#define md5_2_h_

#define BUFFER_SIZE	1024

typedef struct MD5_CTX MD5_CTX;

struct MD5_CTX {
	unsigned int _state[4];
	unsigned int _count[2];
	unsigned char _buffer[64];
	unsigned char _digest[16];
	unsigned char _finished;
};

int MD5Init( struct MD5_CTX * ctx );

int MD5Update( struct MD5_CTX * ctx, const char * input, int len );
int MD5Final( struct MD5_CTX * ctx );

#endif
