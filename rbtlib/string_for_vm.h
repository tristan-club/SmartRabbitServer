#ifndef string_for_vm_h_
#define string_for_vm_h_

#define get_free_str( _r, _sz, result )	\
	result = (TString*)RMALLOC(_r, char, (_sz) + sizeof(TString) + 1);\
	result->len = _sz

#define set_free_str(r, str)	\
	RFREEVECTOR(r, ((char*)(str)), sizeof(TString) + 1 + str->len);

inline void vm_concatenate_str_str_fn(rabbit * r, const TString * ts_a, const TString * ts_b, TValue * out);

inline void vm_concatenate_str_num_fn(rabbit * r, const TString * ts_a, const TValue * num, TValue * out, int flag); 

#endif
