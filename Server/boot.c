#include "boot.h"
#include "svr_connection.h"

int boot_gate(rabbit *r) {
	kLOG(r, 0, "[LOG] Connecting To Gate... \n");
	Connection *c = rbtNet_connect_try_hardly(r, G(r)->gate_ip, G(r)->gate_port);
	kLOG(r, 0, "[LOG] Connected To Gate. \n");
	kLOG(r, 0, "[LOG] Start Registering to Gate... \n");
	G(r)->gate = c;
	rbtRpc_call(r, NULL, c, CONN_FOR_SERVER_REGISTER, "ddd", SERVER_TYPE_WORLD, G(r)->id, G(r)->key);
	struct ConnectionX * connX = rbtNet_get_x(c);
	connX->a1 = CONN_GATE;
	return 0;
}

int rbtF_boot( rabbit * r )
{
	boot_gate(r);
	return 0;
}

