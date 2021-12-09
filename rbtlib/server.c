#include "common.h"

#include "conf.h"
#include "rabbit.h"
#include "network.h"
#include "fdevent.h"

#include "gc.h"

#include <signal.h>

int main( int narg, char* argv[] )
{
	// ignore SIGPIPE
	struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_handler = SIG_IGN;

	sigaction(SIGPIPE,&act,NULL);


	rabbit * r = rabbit_init();

	if(narg > 1) {
		rbtD_init( r );
	}

	if(rbt_config( r ) < 0) {
		kLOG(r, 0, "Config failed.\n");
		exit(1);
	}

	if(network_init( r ) < 0) {
		kLOG(r, 0, "Network Init failed.\n");
		exit(1);
	}

	if(network_set_nonblock( r, r->fd ) < 0) {
		kLOG(r, 0, "SetNonBlock(%d) failed.\n",r->fd);
		exit(1);
	}

	if(fdevent_handler_init( r ) < 0) {
		kLOG(r, 0, "fdevent handler init failed\n");
		exit(1);
	}

	if(logical_process_packet_init( r ) < 0) {
		kLOG(r, 0, "logical process packet init failed.\n");
		exit(1);
	}

	while( 1 ) {
		fdevent_handler_poll( r, 1000 );

		rbtC_step(r, 100);
	}

	return 0;
}

