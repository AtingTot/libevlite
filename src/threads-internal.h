
#ifndef THREADS_INTERNAL_H
#define THREADS_INTERNAL_H

#include "threads.h"

// һ�δӶ�����������ȡ����
#define POP_TASKS_COUNT				128

// ����Ĭ�ϴ�С
#define MSGQUEUE_DEFAULT_SIZE		1024

// �߳�Ĭ��ջ��С
#define THREAD_DEFAULT_STACK_SIZE	(8*1024)

//
// �����߳�
// 

struct iothread
{
	uint8_t 	index;
	pthread_t	id;
	
	evsets_t 	sets;
	void *		parent;

	event_t	 	cmdevent;
	struct msgqueue * queue;
};

int32_t iothread_start( struct iothread * self, uint8_t index, iothreads_t parent );
int32_t iothread_post( struct iothread * self, 
						int16_t type, int16_t utype, void * task, uint8_t size );
int32_t iothread_stop( struct iothread * self );

//
// �����߳���
//

struct iothreads
{
	struct iothread * threadgroup;

	void * context;
	void (*method)( void *, uint8_t, int16_t, void *);	
	
	uint8_t nthreads;
	uint8_t runflags;

	uint8_t nrunthreads;
	pthread_cond_t cond;
	pthread_mutex_t lock;
};


#endif

