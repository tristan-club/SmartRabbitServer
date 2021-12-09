#include "server.h"
#include "boot.h"
#include "process.h"

static void ex_program(int sig)
{
	kLOG(0, 0, "[LOG]Exit Ctrl + C\n");

	exit(0);
}

#define WAIT_TIME	1000

char * program_name;

int main( int narg, char* argv[] )
{
	program_name = argv[0];

	 struct sigaction sa;
	 sa.sa_handler = SIG_IGN;//设定接受到指定信号后的动作为忽略
	 sa.sa_flags = 0;
	 if (sigemptyset(&sa.sa_mask) == -1 || //初始化信号集为空
			 sigaction(SIGPIPE, &sa, 0) == -1) { //屏蔽SIGPIPE信号
		 perror("failed to ignore SIGPIPE; sigaction");
		 exit(1);
	 }

	signal(SIGINT, ex_program);

	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
//	kLOG(NULL, 0, "[LOG]最大能接受的连接数:%d, %d\n", rl.rlim_cur, rl.rlim_max);
	if(rl.rlim_cur < 3000) {
		kLOG(NULL, 0, "[LOG] 最大能接受的连接数小于3000！请重新配置ulimit -n!");
//		exit(1);
	}

	rabbit * r = rbtF_init();

	r->console = 1;

	// 读取配置文件
	Table * config = read_config_from_file( r, "config.conf" );

	if(!config) {
		kLOG(r, 0, "[Error]Config File Parse Error\n");
		exit(1);
	}

	G(r)->config = config;

	// Gate IP && Port
	const TValue *tv = rbtH_getstr(r, config, "gate_ip");
	if(!ttisstr(tv)) {
		kLOG(r, 0, "[Error] Gate IP Is Missing\n");
		exit(1);
	}
	G(r)->gate_ip = inet_addr(rbtS_gets(strvalue(tv)));

	tv = rbtH_getstr(r, config, "gate_port");
	if(!ttisnumber(tv)) {
		kLOG(r, 0, "[Error] Gate Port Is Missing\n");
		exit(1);
	}
	G(r)->gate_port = htons(numbervalue(tv));

	// ID
	tv = rbtH_getstr(r, config, "id");
	if(!ttisnumber(tv) || numbervalue(tv) < 1) {
		kLOG(r, 0, "[Error] ID Is Missing\n");
		exit(1);
	}

	G(r)->id = numbervalue(tv);

	// Key
	tv = rbtH_getstr(r, config, "key");
	if(!ttisnumber(tv)) {
		kLOG(r, 0, "[Error] Router Key Is Missing, default : 0x44551100\n");
		G(r)->key = 0x44551100;
	} else {
		G(r)->key = numbervalue(tv);
	}

	// 初始化网络
	if(rbtNet_init(r) < 0) {
		kLOG(r, 0, "[Error] Net Init Failed\n");
		exit(1);
	}

	// 启动, Listen
	rbtF_boot(r);

	// 进入主循环
	while( 1 ) {
		if(rbtNet_poll(r, WAIT_TIME) < 0) {
			kLOG(r, 0, "[Error] Net poll Failed\n");
			break;
		}

		process(r);

		G(r)->tick++;

		gettimeofday(&G(r)->globalTime, NULL);

		rbtC_auto_gc(r);
	}

	return 0;
}

