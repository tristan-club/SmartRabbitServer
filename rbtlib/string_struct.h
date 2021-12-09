#ifndef string_struct_h_
#define string_struct_h_

struct TString {
	CommonHeader;
	unsigned int hash;
	size_t len;
	int len_exp;	// 1 << len_exp >= len >= 1 << (len_exp - 1)
	struct TString * next;
};

#define gets(str) cast(char*, cast(TString *, str) + 1)

#endif

