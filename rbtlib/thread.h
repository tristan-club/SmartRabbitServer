#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>

/*
 *线程池里所有运行和等待的任务都是一个ThreadJob
 *由于所有任务都在链表里，所以是一个链表结构
 */
typedef struct _thread_job 
{
	/*回调函数，任务运行时会调用此函数，注意也可声明成其它形式*/
	void *(*process) (void *arg);
	void *arg;/*回调函数的参数*/
	struct worker *next;

} ThreadJob;

/*线程池结构*/
typedef struct
{
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_ready;

	/*链表结构，线程池中所有等待任务*/
	ThreadJob *queue_head;

	/*是否销毁线程池*/
	int shutdown;
	pthread_t *threadid;
	/*线程池中允许的活动线程数目*/
	int max_thread_num;
	/*当前等待队列的任务数目*/
	int cur_queue_size;

} ThreadPool;

int rbtT_add_job ( ThreadPool * pool , void *(*process) (void *arg), void *arg);
ThreadPool * rbtT_init( int max_thread_num );

