#include "string_for_vm.h"

inline void vm_concatenate_str_str_fn(Script * S, const TString * ts_a, const TString * ts_b, TValue * out)
{
	rabbit * __r = S->r;
	const TString * ts_a = a;
	const TString * ts_b = b;
	TValue * out = c;
	if((ts_a->len == 0)) {
		setstrvalue_nomark(out, ts_b);
		break;
	}
	if((ts_b->len == 0)) {
		setstrvalue_nomark(out, ts_a);
		break;
	}
	int len = ts_a->len + ts_b->len;
	TString * ts_c;
	get_free_str(__r, len, ts_c);
	memcpy(gets(ts_c), gets(ts_a), ts_a->len);
	memcpy(gets(ts_c) + ts_a->len, gets(ts_b), ts_b->len + 1);
	ts_c->len = len;
	unsigned int hash = len;
	unsigned int step = (len >> 5) + 1;
	for(; len > step; len -= step) {
		hash = hash ^ ((hash<<5) + (hash>>2) + gets(ts_c)[len-1]);
	}
	ts_c->hash = hash;
	if(likely(__r->stbl.size)) {
		size_t pos = hash % __r->stbl.size;
		TString * ts = gco2str(__r->stbl.table[pos]);
		while(ts) {
			if(ts->len == ts_c->len && ts->hash == hash && strncmp(gets(ts), gets(ts_c), ts_c->len) == 0) {
				set_free_str(__r, ts_c);
				setstrvalue_nomark(out, ts);
				return;
			}
			ts = ts->next;
		}
		if(ts) {
			break;
		}
	}
	link_ts(__r, ts_c);
}
