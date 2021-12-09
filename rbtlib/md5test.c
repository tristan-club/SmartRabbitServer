#include "md5_2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int narg, char* argv[])
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	char * str = "zhoukeli-21-zhoukeli";
	MD5Update(&ctx,str,strlen(str));
	MD5Final(&ctx);

	int i;
	for(i = 0; i < 16; i++) {
		fprintf(stderr,"%02x",(unsigned char)ctx._digest[i]);
	}
	fprintf(stderr, "\n");

	return 0;
}
