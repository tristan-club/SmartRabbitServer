#include "common.h"
#include "math.h"
#include "rabbit.h"

inline int rbtM_log2(unsigned int x)
{
	static const char log_2[256] = {
		0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
	};
	int l = -1;
	while (x >= 256) { l += 8; x >>= 8; }
	return l + log_2[x];
}

// 这个字符串最多10位,超出的直接忽略掉..
static inline int s2u( const char *s, int l, int * len , int * real_len)
{
	int r = 0, c = 0;
	int i;
	for(i = 0; i < l; ++i) {
		if(s[i] > '9' || s[i] < '0' || i >= 10) {
			break;
		}
		c = (int)(s[i] - '0');
		r = r * 10 + c;
	}

	if(real_len) {
		*real_len = i;
	}

	while(i < l) {
		if(s[i] >'9' || s[i] <'0') {
			break;
		}
		i++;
	}

	if(len) {
		*len = i;
	}

	return r;
}

inline int stou ( const char* s, int l, int * len ) 
{
	return s2u(s, l, len, NULL);
}

inline int stoi ( const char * s, int l, int * len ) 
{
	if(l <= 0) {
		*len = 0;
		return 0;
	}

	int sign = 0;

	if(s[0] == '-') {
		sign = 1;
	}

	int length;
	int dec = stou(s + sign, l - sign, &length);
	if(len) {
		*len = length + sign;
	}

	return (-2 * sign + 1) * dec;
}

inline double stof( const char * s, int l, int * len )
{	// 该函数可以通过 strtod 来改写 , 去除bug
	if(l <= 0) {
		return 0;
	}

	int sign = 0;
	if(s[0] == '-') {
		sign = 1;
	}

	double ret;

	int pos = 0;
	int dec = s2u(s + sign, l - sign, &pos, NULL);

	if(pos >= l - sign || s[pos] != '.') {
		if(len) {
			*len = pos + sign;
		}
		ret = (double)dec;
		return ret;
	}

	int length,real_len;
	int fct = s2u(s + sign + pos + 1, l - sign - pos, &length, &real_len);

	if(len) {
		*len = length + pos + sign + 1;
	}

	double d = fct;
	while(real_len ) {
	       	d /= 10.0f;
		real_len --;
	}

	ret = d + (double)dec;
	return ret;
}

static inline void reverse( char * p, int len )
{
	if(len <= 1) {
		return;
	}

	int end = len - 1;
	int start = 0;
	int c;
	while(start < end) {
		c = p[end];
		p[end] = p[start];
		p[start] = c;

		start++;
		end--;
	}

	return;
}

static inline int u2s( char * p, int len, int u )
{
	if(len <= 0) {
		return 0;
	}
	if(u <= 0) {
		p[0] = '0';
		return 1;
	}

	int i = 0;
	int c;
	while( u > 0 ) {
		c = u % 10;
		p[i++] = '0' + c;

		u /= 10;

		if(i >= len) {
			break;
		}
	}

	reverse(p,i);

	return i;
}

inline char * utos( int u )
{
	static char buf[64];
	memset(buf,0,64 * sizeof(char));

	u2s(buf,63,u);

	return buf;
}

inline char * itos( int s )
{
	static char buf[64];
	memset(buf,0,64 * sizeof(char));

	int sign = 0;
	if(s < 0) {
		sign = 1;
		buf[0] = '-';
		s *= -1;
	}

	u2s(buf + sign, 63 - sign, s);

	return buf;
}

inline char * ftos( double f )
{
	static char buf[128];
	memset(buf,0,128 * sizeof(char));

	snprintf(buf, 127, "%f", f);

	return buf;
}

enum {
	EVAL_TOKEN_ONE,                         // 个
	EVAL_TOKEN_TEN,                         // 十
	EVAL_TOKEN_HUNDRAND,                    // 百
	EVAL_TOKEN_THOUSAND,                    // 千
	EVAL_TOKEN_TEN_THOUSAND,                // 万
	EVAL_TOKEN_HUNDRAND_MILLION,            // 亿
	EVAL_TOKEN_NOT_OP,

	EVAL_TOKEN_ERROR,
	EVAL_TOKEN_END_FILE,

	EVAL_TOKEN_NUMBER,
};

#define IS_OP(token)    (token > EVAL_TOKEN_ONE && token < EVAL_TOKEN_NOT_OP)

static char * numbers[] = {
	"零", "一", "二", "三", "四", "五", "六", "七", "八", "九", NULL
};

static char * denotions[] = {
	"个", "十", "百", "千", "万", "亿", NULL
};

struct eval_token {
	int token;
	int val;
};

struct eval_lex {
	const char * p;
	int pos;
	int len;

	struct eval_token token;
};

static int _eval_get_token( struct eval_lex * lex )
{
	if(lex->pos == lex->len) {
		lex->token.token = EVAL_TOKEN_END_FILE;
		return lex->token.token;
	}

	int c = lex->p[lex->pos];

	if(c >= '0' && c <= '9') {
		int len;
		lex->token.val = stou(&lex->p[lex->pos], lex->len - lex->pos, &len);
		lex->pos += len;
		lex->token.token = EVAL_TOKEN_NUMBER;

		return EVAL_TOKEN_NUMBER;
	}

	int i;
	for(i = 0; numbers[i]; ++i) {
		int len = strlen(numbers[i]);
		if(strncmp(numbers[i], &lex->p[lex->pos], len) == 0) {
			lex->token.val = i;
			lex->token.token = EVAL_TOKEN_NUMBER;

			lex->pos += len;

			return EVAL_TOKEN_NUMBER;
		}
	}

	for(i = 0; denotions[i]; ++i) {
		int len = strlen(denotions[i]);
		if(strncmp(denotions[i], &lex->p[lex->pos], len) == 0) {
			lex->token.token = i;

			lex->pos += len;
			return i;
		}
	}

	lex->token.token = EVAL_TOKEN_ERROR;

	return EVAL_TOKEN_ERROR;
}

static int _eval_sub( struct eval_lex * lex, int last_op )
{
	_eval_get_token(lex);

	if(lex->token.token == EVAL_TOKEN_END_FILE) {
		return 0;
	}

	if(lex->token.token != EVAL_TOKEN_NUMBER) {
		kLOG(NULL, 0, "[Error]Math.eval(%s). Begin Invalid\n", lex->p);
		return -1;
	}

	int a = lex->token.val;

	int op = _eval_get_token(lex);

	if(!IS_OP(op)) {
		if(op == EVAL_TOKEN_END_FILE) {
			return a;
		}

		kLOG(NULL, 0, "[Error]Math.eval(%s). Wait For OP\n", lex->p);
		return -1;
	}

	while(op < last_op) {
		int b = _eval_sub(lex, op);

		switch(op) {
			case EVAL_TOKEN_TEN:
				a = a * 10 + b;

				break;

			case EVAL_TOKEN_HUNDRAND:
				a = a * 100 + b;

				break;

			case EVAL_TOKEN_THOUSAND:
				a = a * 1000 + b;

				break;

			case EVAL_TOKEN_TEN_THOUSAND:
				a = a * 10000 + b;

				break;

			case EVAL_TOKEN_HUNDRAND_MILLION:
				a = a * 100000000 + b;

				break;

			default:
				break;

		}

		op = lex->token.token;
	}

	return a;
}

int eval( const char * buf )
{
	struct eval_lex lex;
	memset(&lex, 0, sizeof(lex));

	lex.p = buf;
	lex.len = strlen(buf);

	return _eval_sub(&lex, EVAL_TOKEN_NOT_OP);
}


