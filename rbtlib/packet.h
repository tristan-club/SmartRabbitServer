#ifndef packet_h_
#define packet_h_

#include "object.h"

#define PKT_ENCODE_MASK	0x80
#define PKT_IS_ENCODE(fun)	(fun & PKT_ENCODE_MASK)
#define PKT_HEAD_ENCODE_MASK	0x40
#define PKT_IS_HEAD_ENCODE(fun)	(fun & PKT_HEAD_ENCODE_MASK)

#define PKT_FUN_MASK		0x8000
#define PKT_IS_FUN(fun)		(fun & PKT_FUN_MASK)

#define PKT_REQ_MASK		0x4000
#define PKT_HAS_REQ(fun)	(fun & PKT_REQ_MASK)

#define PKT_FUN_VALUE(fun)	(fun & 0x3FFF)

#define PKT_REQ_SIZE		15
#define PKT_REQ_VALUE(fun)	(fun & 0x7FFF)
#define PKT_REQ_MAX		32767

#define PKT_FUN_MAX		16383

#define PACKET_LEN_POS	0
#define PACKET_LEN_SIZE	3
#define PACKET_FLAG_POS	(PACKET_LEN_POS + PACKET_LEN_SIZE)
#define PACKET_FLAG_SIZE	1
#define PACKET_FUN_POS	(PACKET_FLAG_POS + PACKET_FLAG_SIZE)
#define	PACKET_FUN_SIZE	2
#define PACKET_SEQ_POS	(PACKET_FUN_POS + PACKET_FUN_SIZE)
#define PACKET_SEQ_SIZE	2
#define PACKET_HEAD_SIZE	(PACKET_LEN_SIZE + PACKET_FLAG_SIZE + PACKET_FUN_SIZE + PACKET_SEQ_SIZE)	

#define PKT_DATA_SIZE(pkt)	(rbtP_size(pkt) - PACKET_HEAD_SIZE)

#define rbtP_head_encode(pkt)	\
	do {	\
		char flag = rbtP_get_flag(pkt);	\
		if (PKT_IS_HEAD_ENCODE(flag))	{	\
			break;	\
		}	\
		flag |= PKT_HEAD_ENCODE_MASK;	\
		rbtP_set_flag(pkt, flag);	\
		rbtP_raw_head_encode(pkt);	\
	}while(0)

#define rbtP_head_decode(pkt)	\
	do {	\
		char flag = rbtP_get_flag(pkt);	\
		if (PKT_IS_HEAD_ENCODE(flag) == 0)	{	\
			break;	\
		}	\
		flag &= ~PKT_HEAD_ENCODE_MASK;	\
		rbtP_set_flag(pkt, flag);	\
		rbtP_raw_head_encode(pkt);	\
	}while(0)

#define rbtP_encode(pkt)	\
	do {	\
		char flag = rbtP_get_flag(pkt);	\
		if(PKT_IS_ENCODE(flag)) {	\
			break;	\
		}	\
		flag |= PKT_ENCODE_MASK;	\
		rbtP_set_flag(pkt, flag);	\
		rbtP_raw_encode(pkt);	\
	}while(0)

#define rbtP_decode(pkt)	\
	do {	\
		char flag = rbtP_get_flag(pkt);	\
		if(PKT_IS_ENCODE(flag) == 0) {	\
			break;	\
		}	\
		flag &= ~PKT_ENCODE_MASK;	\
		rbtP_set_flag(pkt, flag);	\
		rbtP_raw_encode(pkt);	\
	}while(0)

/*
 *	系统共有2个特殊的全局包：
 *
 *	(1) Flash 发来的安全沙盒包，对于这个包会进行自动处理
 *	(2) 超大包，如果连接发送超大包，默认会断开连接
 */
extern const Packet * rbtP_FlashSecurityPacket;
extern const Packet * rbtP_HugePacket;
extern const Packet * rbtP_QQ_TGW_Packet;


/*
 *	数据包系统 初始化，主要是构造全局的特殊包，要在 接收/发送 任何包之前调用
 *
 *	@param r
 */
void rbtPacket_init( rabbit * r );

/*
 *	AddRef
 *
 */
void rbtP_grab( struct Packet * pkt );

void rbtP_drop( struct Packet * pkt );

/*
 *	读取/设置 包的基本信息
 *
 *	@param pkt
 */
char rbtP_get_flag( struct Packet * pkt );
void rbtP_set_flag( struct Packet * pkt, char flag );

int rbtP_get_fun( struct Packet * pkt );
void rbtP_set_fun( struct Packet * pkt, int fun );

short int rbtP_get_seq( struct Packet * pkt );
void rbtP_set_seq( struct Packet * pkt, short int seq );


/*
 *	删除包末尾len个字节
 *
 */
void rbtP_erase(struct Packet * pkt, int len);

/*
 *	获得一个 Packet 的 raw data 的 size
 *
 *	@param pkt
 */
int rbtP_size( const struct Packet * pkt );


/*
 *	从接收的一段数据中，分离出一个包来，失败返回NULL
 *
 *	@param r
 *	@param p	-- 数据
 *	@param len	-- 数据长度
 */
Packet * rbtP_parse( rabbit * r, char * p, int len );


/*
 *	新建一个空包
 *
 *	@param r
 */
Packet * rbtP_init( rabbit * r );


/*
 *	调整 Packet 里的 读写 指针位置（从0开始）
 *
 *	@param pkt
 *	@param offset
 *	@ret 当前位置
 */
void rbtP_seek( Packet * pkt, int offset );
void rbtP_seek_end( struct Packet * pkt );

int rbtP_curr_offset( Packet * pkt );


/*
 *	向一个包里写入一个字节
 *
 *	@param pkt
 *	@param c
 */
int rbtP_writeByte( Packet * pkt, unsigned char c );


/*
 *	向一个包里写入一个4字节的整数
 *
 *	@param pkt
 *	@param i
 */
int rbtP_writeInt( Packet * pkt, int i );

// 写入一个AMF3压缩的int
int rbtP_writeIntAMF3( Packet * pkt, int i );
int rbtP_readIntAMF3( Packet * pkt, int * i );

/*
 *	向一个包里写入一个2字节的整数
 *
 *	@param pkt
 *	@param u
 */

int rbtP_writeShort( Packet * pkt, short int u );


/*
 *	写入一个8字节的浮点数
 *
 *	@param pkt
 *	@param d
 */
int rbtP_writeDouble( Packet * pkt, double d );


/*
 *	写入一个 Table，以AMF3的格式写入
 *
 *	@param pkt
 *	@param tbl
 */
int rbtP_writeTable( Packet * pkt, const Table * tbl );

/*
 *	写入一个 C String，UTF-8格式
 *
 *	@param pkt
 *	@param p
 */
int rbtP_writeString( Packet * pkt, const char * p );

/*
 *	写入一定长度的字符串
 *
 *	@param pkt
 *	@param p
 *	@param len
 */
int rbtP_writeStringLen( Packet * pkt, const char * p, int len );

/*
 *	写入一个 TValue
 *
 *	@param pkt
 *	@param tv
 */
int rbtP_writeTValue( Packet * pkt, const TValue * tv );

int rbtP_readTValue( Packet * pkt, TValue * tv );



/*
 *	从数据包中读取一个4字节的整数，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param i
 */
int rbtP_readInt( Packet * pkt, int * i );


/*
 *	从数据包里读取一个2字节的整数，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param u
 */
int rbtP_readShort( Packet * pkt, short int * u );


/*
 *	从数据包里读取一个8字节的浮点型，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param d
 */
int rbtP_readDouble( Packet * pkt, double * d );


/*
 *	读取一个字节，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param c
 */
int rbtP_readByte( Packet * pkt, unsigned char * c );


/*
 *	读取一个UTF-8字符串，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param ts
 */
int rbtP_readString( Packet * pkt, TString ** ts );

/*
 *	读取一定长度的字符串，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param p
 *	@param len
 */
int rbtP_readStringLen( Packet * pkt, TString ** ts, int len );

/*
 *	读取一个 AMF3 格式的Table，返回0（成功），-1（失败）
 *
 *	@param pkt
 *	@param tbl
 */
int rbtP_readTable( Packet * pkt, Table ** tbl );


/*
 *	用 LZW 算法压缩数据包
 *
 *	@param pkt
 */
int rbtP_compress( Packet * pkt );

/*
 *	Decode / Encode
 */
void rbtP_raw_encode ( Packet * pkt );
void rbtP_raw_head_encode( Packet * pkt );

/*
 *	打印一个数据包的信息
 *
 *	@param pkt
 */
void rbtD_packet(Packet * pkt);

int rbtM_npkt();
int rbtM_mpkt();

#endif

