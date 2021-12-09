#include "open_login.h"
#include <time.h>
#include <string.h>
#include "../rbtlib/md5.h"

static int check_md5( const char * input , const char * input_md5 ){

	struct MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, input, strlen(input));
	MD5Final(&ctx);

	static char HEX[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	char buf[33];
	int i;

	for(i = 0;i < 16; ++i) {
		buf[2 * i] = HEX[(ctx._digest[i]) >> 4];
		buf[2 * i + 1] = HEX[(ctx._digest[i]) & 0xF];
	}

	buf[32] = 0;

	if( strncmp( buf , input_md5 , 32 ) == 0 ){
		return 1;
	}	

	return 0;
}

int get_account_from_open_ticket( const unsigned char * buf ){
	
	char check_buf[1024] = {0};
	
	char * p = check_buf;

	char * dup_str = strdup( buf );

	char * pch = strtok( dup_str, ",");

	int account_id;

	if( pch ){

		account_id = atoi( pch );

		strncpy( check_buf , pch , 20 );	//account 
		
		p += strlen( pch );
	}else{

		free( dup_str );

		return 0;
	}

	pch = strtok( NULL , "," );

	if( pch ){
		int time = atoi(pch);
/*
		time_t now = time(NULL);

		if( now - time > 20 * 60 ){
			//key过期
			free( dup_str );
			return 0;
		}	
		*/

		strncpy( p , pch , 12 ); //unix时间戳，给个12字符上限够了
		
		p += strlen( pch );
	}else{
		free( dup_str );
		return 0;
	}

	pch = strtok( NULL , "," );

	if( !pch ){
		free( dup_str );
		return 0;
	}

	char * sign = pch;

	strncpy( p , VERIFY_SECRET_KEY , strlen(VERIFY_SECRET_KEY));

	if( check_md5( check_buf , sign ) ){
		free( dup_str );
		return account_id;
	}
			
	free( dup_str );
	return 0;
}
