#ifndef io_h_
#define io_h_

#include "common.h"

// 遇到了文件尾
#define IO_ERROR_READ_EOF	-1
// 内存不足
#define IO_ERROR_WRITE_NO_MEM	-2
// 参数错误
#define IO_ERROR_BAD_PARAM	-3

struct i_io {
	char 		( *read_char )	( struct i_io * io );
	short int 	( *read_short )	( struct i_io * io );
	int		( *read_int )	( struct i_io * io );
	int		( *read_len )	( struct i_io * io, char * out, int len );
	int		( *read_len_io )( struct i_io * io, struct i_io * out, int len );

	int		( *write_char ) ( struct i_io * io, char c );
	int		( *write_short) ( struct i_io * io, short int i );
	int		( *write_int)	( struct i_io * io, int i );
	int		( *write_len)	( struct i_io * io, const char * in, int len );
	int		( *write_len_io)( struct i_io * io, struct i_io * in, int len );

	void		( *seek )	( struct i_io * io, int at );
	int		( *tell )	( struct i_io * io );
	int		( *eof )	( struct i_io * io );
	int 		( *size )	( struct i_io * io );

	void		( *erase)	( struct i_io * io, int len );	// 删掉最后面len个字节

	int		( *error )	( struct i_io * io );	// 上次操作是否出错 0 : suc, 1 : fail
	void 		( *encode )	(struct i_io * io, int len);
};

struct i_io *
io_create( rabbit * r );

const char *
io_get_p( struct i_io * );

void 
io_destroy( struct i_io * );

struct io_debug {
	int nio;
	int mio;
};

struct io_debug * io_get_debug();

#endif

