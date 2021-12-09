#include "packet.h"
#include "packet_struct.h"

#include "fill_stream.h"

#include "rabbit.h"
#include "string.h"
#include "gc.h"
#include "mem.h"
#include "amf.h"
#include "amf_common.h"
#include "util.h"

//#include "../Compress/interface.h"

const Packet * rbtP_FlashSecurityPacket;
const Packet * rbtP_HugePacket;
const Packet * rbtP_QQ_TGW_Packet;

static int g_nPkt = 0;

int rbtM_npkt()
{
	return g_nPkt;
}

int rbtM_mpkt()
{
	return mblock_debug_mem();
}

#define LEN_BITS_SIZE	23
#define ISVALID(p) ((p[0] & 0x80) != 0)
#define REDISPKT(p) ((p[0] & 0x58) != 0)
#define LENGTH(p) ( ((p[PACKET_LEN_POS] << 16) & 0x7F0000) | ((p[PACKET_LEN_POS + 1] << 8) & 0xFF00) | (p[PACKET_LEN_POS + 2] & 0xFF ) )

#define DATA_START_POSITION	PACKET_HEAD_SIZE
#define PACKET_ENCODE_START	DATA_START_POSITION	

#define MIN_PACKET_SIZE PACKET_HEAD_SIZE
#define MAX_PACKET_SIZE ((1 << LEN_BITS_SIZE) - 1)

inline int rbtP_size( const struct Packet * pkt )
{
	return pkt->io.size;
}

void rbtP_grab( Packet * pkt )
{
	pkt->count++;
}

void rbtP_drop( Packet * pkt )
{
	if(!pkt || pkt == rbtP_FlashSecurityPacket || pkt == rbtP_HugePacket || pkt == rbtP_QQ_TGW_Packet) {
		return;
	}

	if(pkt->count <= 0) {
		kLOG(pkt->io.r, 0, "[Error]一个Packet 多次Drop！\n");
		return;
	}

	pkt->count--;

	if(pkt->count <= 0) {
		pkt->io.r->obj--;

		g_nPkt--;
		mblock_free_list(cast(struct MBlock *, pkt));
	}
}

Packet * rbtP_init( rabbit * r )
{
	r->obj++;

	g_nPkt++;

	struct MBlock * block = mblock_create(r);
	Packet * pkt = cast(Packet *, block);

	pkt->io.r = r;
	struct i_io * io = mblock_io(&pkt->io, block, sizeof(Packet));

	pkt->count = 1;
	//pkt->is_encode = 0;
	//pkt->is_decode = 0;

	io->write_char(io, 0x80);	// 长度为8
	io->write_char(io, 0x00);
	io->write_char(io, 0x08);
	io->write_char(io, 0x00);
	io->write_char(io, 0x00);
	io->write_char(io, 0x00);
	io->write_char(io, 0x00);
	io->write_char(io, 0x00);

	return pkt;
}

Packet * rbtP_parse(rabbit * r, char * p, int len)
{
	if( len < MIN_PACKET_SIZE) {
		kLOG(r, 0, "[Error]Packet Parse. Len(%d) is too little\n", len);
		return NULL;
	}

	if(strncmp(p, "<xml", 4) == 0 || strncmp(p, "<pol", 4) == 0) {
		return cast(Packet *, rbtP_FlashSecurityPacket);
	}
	if(strncmp(p, "tgw_l7_forward", 14) == 0) {
		kLOG(r, 0, "Receive TGW Packet!\n");
		return cast(Packet *, rbtP_QQ_TGW_Packet);
	}

	if(!ISVALID(p)){
		/* 判断是不是redis发送的包 */
		if ( !REDISPKT(p) ) {
			kLOG(r, 0, "[Error]Packet Parse. It is not a valid packet, start with(%u)\n", p[0]);
		}

		return NULL;
	}

	int pkt_len = LENGTH(p);

	if(pkt_len == 0) {
		kLOG(r, 0, "[Error]Packet Parse. Zero Len packet\n");

		return cast(Packet *, rbtP_HugePacket);
	}

	if( pkt_len > MAX_PACKET_SIZE ) {
		kLOG(r, 0, "[Error]Packet Parse. HugePacket:%d\n", pkt_len);
		return cast(Packet *, rbtP_HugePacket);
	}

	if( pkt_len > len ) {
		return NULL;
	}

	Packet * pkt = rbtP_init(r);

	struct i_io * io = cast(struct i_io *, &pkt->io);

	io->seek(io, 0);
	
	io->write_len(io, p, pkt_len);

	io->seek(io, DATA_START_POSITION);

	return pkt;
}

static int reset_size(struct Packet * pkt)
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_LEN_POS);

	int len = io->size(io);

	if(len > MAX_PACKET_SIZE) {
		kLOG(pkt->io.r, 0, "[Error]数据包长度过长！！(%d)\n", len);
		assert(0);
	}

	io->write_char(io, (len >> 16) | 0x80);
	io->write_char(io, (len >> 8) & 0xFF);
	io->write_char(io, len & 0xFF);

	io->seek(io, now);

	return len;
}

inline char rbtP_get_flag( struct Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_FLAG_POS);

	char flag = io->read_char(io);

	io->seek(io, now);

	return flag;
}

inline void rbtP_set_flag( struct Packet * pkt, char flag)
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	
	int now = io->tell(io);

	io->seek(io, PACKET_FLAG_POS);
	io->write_char(io, flag);

	io->seek(io, now);
}

inline int rbtP_get_fun( struct Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_FUN_POS);

	int fun = io->read_short(io);

	io->seek(io, now);

	return fun;
}

inline void rbtP_set_fun( struct Packet * pkt, int fun )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	
	int now = io->tell(io);

	io->seek(io, PACKET_FUN_POS);
	io->write_short(io, fun);

	io->seek(io, now);
}

inline short int rbtP_get_seq( struct Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_SEQ_POS);

	short int seq = io->read_short(io);

	io->seek(io, now);

	return seq;
}

inline void rbtP_set_seq( struct Packet * pkt, short int seq)
{

	struct i_io * io = cast(struct i_io *, &pkt->io);
	
	int now = io->tell(io);

	io->seek(io, PACKET_SEQ_POS);
	io->write_short(io, seq);

	io->seek(io, now);
}

inline void rbtP_seek( struct Packet * pkt, int offset )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	io->seek(io, offset + DATA_START_POSITION);
}

inline void rbtP_seek_end( struct Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	io->seek(io, io->size(io));
}

inline int rbtP_curr_offset( struct Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	return io->tell(io) - DATA_START_POSITION;
}

int rbtP_writeInt( Packet * pkt, int i )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	io->write_int(io, i);
	reset_size(pkt);

	return 0;
}

int rbtP_writeIntAMF3(Packet * pkt, int i)
{
	if(i > MAX_INT || i < MIN_INT) {
		kLOG(pkt->io.r, 0, "[Error] Packet.writeIntAmf3 Error. 数字(%d)太大或太小！\n", i);
		return -1;
	}
	i &= 0x1FFFFFFF;

	char tmp[4];
	int size = 0;

	if(i < 0x80) {
		tmp[0] = i & 0x7F;
		size = 1;
	} else if (i < 0x4000) {
		tmp[0] = (i >> 7 & 0x7F) | 0x80;
		tmp[1] = i & 0x7F;
		size = 2;
	} else if (i < 0x200000) {
		tmp[0] = (i >> 14 & 0x7F) | 0x80;
		tmp[1] = (i >> 7 & 0x7F) | 0x80;
		tmp[2] = i & 0x7F;
		size = 3;
	} else {
		tmp[0] = (i >> 22 & 0x7F) | 0x80;
		tmp[1] = (i >> 15 & 0x7F) | 0x80;
		tmp[2] = (i >> 8 & 0x7F) | 0x80;
		tmp[3] = i & 0xFF;
		size = 4;
	}

	struct i_io * io = cast(struct i_io *, &pkt->io);
	io->write_len(io, tmp, size);
	reset_size(pkt);

	return 0;
}

int rbtP_readIntAMF3(Packet * pkt, int * i)
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int size = 0;
	int tmp = 0;

	while(size < 4) {
		char c = io->read_char(io);
		if(io->error(io)) {
			return -1;
		}
		
		if(size < 3) {
			tmp = (tmp << 7) | (c & 0x7F);
		} else {
			tmp = (tmp << 8) | (c & 0xFF);
		}

		size++;

		if((c & 0x80) == 0) {
			break;
		}
	}

	if(tmp & 0x10000000) {
		tmp -= 0x20000000;
	}

	*i = tmp;

	return 0;
}

int rbtP_writeDouble( Packet * pkt, double d )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	union aligned {
		double d_value;
		char c_value[8];
	} d_aligned;
	char *char_value = d_aligned.c_value;
	d_aligned.d_value = d;

	if (is_bigendian()) {
		return io->write_len(io, char_value, 8);
	} else {
		char flipped[8] = {char_value[7], char_value[6], char_value[5], char_value[4], char_value[3], char_value[2], char_value[1], char_value[0]};
		return io->write_len(io, flipped, 8);
	}

	reset_size(pkt);

	return 0;
}

int rbtP_writeShort( Packet * pkt, short int u )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	io->write_short(io, u);

	reset_size(pkt);

	return 0;
}

int rbtP_writeTable( Packet * pkt, const Table * tbl )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	TValue tv;
	settblvalue(&tv, tbl);

	rbtAMF_encode(pkt->io.r, &tv, io);

	reset_size(pkt);

	return 0;
}

int rbtP_writeTValue( Packet * pkt, const TValue * tv )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	rbtAMF_encode(pkt->io.r, tv, io);

	reset_size(pkt);

	return 0;
}

int rbtP_readTValue( Packet * pkt, TValue * tv )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	return rbtAMF_decode(pkt->io.r, io, tv);
}

/*
 *	Length(short int) + String(lenght of char)
 *
 *	XXX
 */
int rbtP_writeString( Packet * pkt, const char * str )
{
	int len = strlen(str);
	if(len >= 1 << 15) {
		kLOG(pkt->io.r, 0, "[Error]往数据包里写数据过长！(%d) >= (%d)\n", len, 1 << 15);
		return -1;
	}

	struct i_io * io = cast(struct i_io *, &pkt->io);

	io->write_short(io, len);
	io->write_len(io, str, len);

	reset_size(pkt);

	return 0;
}

int rbtP_writeStringLen( Packet * pkt, const char * str, int len )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	if(len >= 1 << 15) {
		kLOG(pkt->io.r, 0, "[Error]往数据包里写数据过长！(%d) >= (%d)\n", len, 1 << 15);
		return -1;
	}
	io->write_short(io, len);
	io->write_len(io, str, len);

	reset_size(pkt);

	return 0;
}


int rbtP_readInt( Packet * pkt, int * i )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	if(io->size(io) - io->tell(io) < 4) {
		return -1;
	} 
	int v = io->read_int(io);
	*i = v;

	return 0;
}

int rbtP_readShort( Packet * pkt, short int * u )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	if(io->size(io) - io->tell(io) < 2) {
		return -1;
	}

	short int v = io->read_short(io);
	*u = v;

	return 0;
}

int rbtP_readDouble( Packet * pkt, double * pd )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	if(io->size(io) - io->tell(io) < 8) {
		return -1;
	}

	char bytes[8];
	if(io->read_len(io, bytes, 8) != 8) {
		return -1;
	}

	// Put bytes from byte array into double
	union aligned {
		double d_val;
		char c_val[8];
	} d;

	if (is_bigendian()) {
		memcpy(d.c_val, bytes, 8);
	} else {
		// Flip endianness
		d.c_val[0] = bytes[7];
		d.c_val[1] = bytes[6];
		d.c_val[2] = bytes[5];
		d.c_val[3] = bytes[4];
		d.c_val[4] = bytes[3];
		d.c_val[5] = bytes[2];
		d.c_val[6] = bytes[1];
		d.c_val[7] = bytes[0];
	}

	*pd = d.d_val;

	return 0;
}

// 先读长度（short int），再跟数据
int rbtP_readString( Packet * pkt, TString ** ts )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int len = io->read_short(io);

	*ts = cast(TString*, rbtS_init_io(pkt->io.r, io, len));
	if(!(*ts)) {
		return -1;
	}

	return 0;
}

int rbtP_readTable( Packet * pkt, Table ** tbl )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	TValue tv;
	rbtAMF_decode(pkt->io.r, io, &tv);

	if(!ttistbl(&tv)) {
		return -1;
	}

	if(tbl) {
		*tbl = tblvalue(&tv);
	}

	return 0;
}

void rbtP_erase(Packet * pkt, int len)
{
	struct i_io * io = cast(struct i_io *, &pkt->io);
	io->erase(io, len);

	reset_size(pkt);
}

void rbtPacket_init( rabbit * r )
{
	char * xml = "<cross-domain-policy>"
		"<allow-access-from domain=\"*\" to-ports=\"*\" />"
		"</cross-domain-policy>";

	int xml_length = strlen(xml) + 1;

	Packet * pkt = rbtP_init(r);
	struct i_io * io = cast(struct i_io *, &pkt->io);
	io->seek(io, 0);
	io->write_len(io, xml, xml_length);

	rbtP_FlashSecurityPacket = pkt;

	rbtP_HugePacket = rbtP_init(r);

	char * qq_tgw = "tgw_l7_forward\r\nHost: gate.app100666137.twsapp.com:8001\r\n\r\n";
	int qq_tgw_len = strlen(qq_tgw);

	pkt = rbtP_init(r);
	io = cast(struct i_io *, &pkt->io);
	io->seek(io, 0);
	io->write_len(io, qq_tgw, qq_tgw_len);

	rbtP_QQ_TGW_Packet = pkt;
}

void rbtP_raw_encode( Packet * pkt )
{
	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_ENCODE_START);
	io->encode(io, -1);

        io->seek(io, now);
}

void rbtP_raw_head_encode( Packet * pkt)
{
	int offset = 13;
	int key_len = PACKET_FUN_SIZE + PACKET_SEQ_SIZE;
	int charpos = DATA_START_POSITION + offset;
	int i;

        static const char * default_key = "R#1k";
	static const char * encode_key = "r*J6";
	char fun_and_seq[PACKET_FUN_SIZE + PACKET_SEQ_SIZE];
	char keychar;

	struct i_io * io = cast(struct i_io *, &pkt->io);

	int now = io->tell(io);

	io->seek(io, PACKET_FUN_POS);

	if (io->read_len(io, fun_and_seq, key_len) != key_len) {
		kLOG(pkt->io.r, 0, "[Error]Can not read fun and seq\n");
		return;
	}

	if (rbtP_size(pkt) <= charpos) {
		for (i = 0 ; i < key_len ; i++) {
			fun_and_seq[i] ^= default_key[i];
		}
	}else {
		io->seek(io, charpos);
		keychar = io->read_char(io);
		for (i = 0 ; i < key_len ; i++) {
			fun_and_seq[i] ^= keychar ^ encode_key[i];
		}
	}

	io->seek(io, PACKET_FUN_POS);
	io->write_len(io, fun_and_seq, key_len);

	io->seek(io, now);
}

void rbtD_packet( Packet * pkt )
{
	int curr = rbtP_curr_offset(pkt);

	rbtP_seek(pkt, 0);

	int size = rbtP_size(pkt);
	int fun = rbtP_get_fun(pkt);
	unsigned char c;
	fprintf(stderr, "[Packet Dump], len:%d, fun:(%x,%x)\n", size, fun & 0xFF, (fun >> 8) & 0xFF);

	int i;
	struct i_io * io = cast(struct i_io *, &pkt->io);
	for(i = 0; i < size - PACKET_HEAD_SIZE; ++i) {
		c = io->read_char(io);
		fprintf(stderr, "%2x\t", c);
		if((i % 10) == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n\n");

	rbtP_seek(pkt, curr);
}
