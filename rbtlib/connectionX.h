#ifndef connectionx_h_
#define connectionx_h_

struct ConnectionX {
	union {
		void * p1;
		int a1;
	};
	union {
		void * p2;
		int a2;
	};
};

#endif

