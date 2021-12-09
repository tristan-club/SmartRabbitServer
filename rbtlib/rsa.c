#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "rsa.h"
#include "base64.h"

static RSA * read_rsa_key(){

	static RSA * rsa = NULL;
	
	if( rsa ){
		return rsa;
	}

	FILE *keyfile = fopen("pub.pem", "r");

	if( !keyfile ){
		printf("[Error]no pub.pem file\n");
		return NULL;
	}

	rsa = PEM_read_RSA_PUBKEY(keyfile, NULL, NULL, NULL);
	
	if (rsa == NULL)
	{
		printf("[Error]rsa decrypt! read key error\n");
		RSA_free( rsa );
		return NULL;
	}

	return rsa;
}


unsigned char * rsa_decode( const unsigned char * buf , int buf_len , int * result_len){

	unsigned char * b64_de =  base64_decode( buf , buf_len , result_len );

	if( !b64_de ){
		printf("[Error]rsa b64 decode error\n");
		return NULL;
	}

	unsigned char* decrypted = (unsigned char *) malloc( *result_len );

	if( !decrypted ){

		printf("[Error]rsa derypt error ,not enough memory");

		free(b64_de);

		return NULL;
	}

	RSA *rsa = read_rsa_key();

	if ( (*result_len = RSA_public_decrypt( *result_len, b64_de , decrypted, rsa, RSA_PKCS1_PADDING) ) != -1 ){

		free(b64_de);

		return decrypted;
	}
	else{
		printf("[Error]rsa decrypt error! decryption failed\n");
		goto rt;
	}

rt:
	free(b64_de);

	free( decrypted );

	return NULL;
}
