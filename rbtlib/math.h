#ifndef _math_h_
#define _math_h_

inline int rbtM_log2( unsigned int x );

inline int stou( const char * str, int len, int * l );
inline int stoi( const char * str, int len , int * l );
inline double stof( const char * str, int len, int * l );

inline char * utos( int u );
inline char * itos( int s );
inline char * ftos( double f );

int eval( const char * str );

#define rbtM_pow2(x) (1 << x)

#undef min
#define min(x,y) ((x) > (y) ? (y) : (x))

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#endif
