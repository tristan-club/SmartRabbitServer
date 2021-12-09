#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "login.h"
#include "../rbtlib/base64.h"

unsigned char * login_decode_ticket( unsigned char * buf , int buf_len , int * result_len){
	
	unsigned char * b64_de =  base64_decode( buf , buf_len , result_len );

	if( *result_len < 0 ){
		printf("login b64 decode error\n");
		return NULL;
	}

	unsigned char* decrypted = (unsigned char *) malloc( *result_len );

	if( !decrypted ){

		printf("login derypt error ,not enough memory");
		return NULL;
	}

	FILE *keyfile = fopen("pub.pem", "r");
	
	RSA *rsa = PEM_read_RSA_PUBKEY(keyfile, NULL, NULL, NULL);

	fclose( keyfile );

	if (rsa == NULL)
	{
		printf("login error! Did not read key file\n");
		goto rt;
	}

	if ( (*result_len = RSA_public_decrypt( *result_len , b64_de , decrypted, rsa, RSA_PKCS1_PADDING)) != -1){

		printf("\nMessage decrypted to : %s, length:%d", decrypted, *result_len);
		RSA_free( rsa );
		return decrypted;
	}
	else{
		printf("login error! decryption failed\n");
		goto rt;
	}

rt:
	free( decrypted );

	RSA_free(rsa);

	return NULL;
}
