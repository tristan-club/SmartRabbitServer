#ifndef vm_h_
#define vm_h_

#include "script.h"

/*
 *	每次运行都有一个运行时环境，从上次的运行时环境继续运行
 *
 *	@param S
 */
int vm_execute( Script * S );

/*
int vm_execute_context_resume( Script * S, struct vm_context * ctx );
int vm_execute_context_do( Script * S, struct vm_context * ctx );
*/

#endif
