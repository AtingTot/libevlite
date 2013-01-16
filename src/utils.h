
#ifndef SRC_UTILS_H
#define SRC_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * �����㷨ģ��
 */

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/uio.h>

#include "queue.h"
#include "network.h"

//
// ϵͳ��صĲ���
//

// ʱ�亯��, ���غ�����
int64_t mtime();

// socket��������
int32_t is_connected( int32_t fd );
int32_t set_non_block( int32_t fd );
int32_t tcp_accept( int32_t fd, char * remotehost, uint16_t * remoteport );
int32_t tcp_listen( char * host, uint16_t port, void (*options)(int32_t) );
int32_t tcp_connect( char * host, uint16_t port, void (*options)(int32_t) );

//
// �����㷨��
//
uint32_t getpower( uint32_t size );
uint32_t nextpow2( uint32_t size );

/*
 * sidlist 
 */
struct sidlist
{
	uint32_t	count;
	uint32_t	size;

	sid_t *		entries;
};

struct sidlist * sidlist_create( uint32_t size );
#define sidlist_count( self )	( (self)->count )
sid_t sidlist_get( struct sidlist * self, int32_t index );
int32_t sidlist_add( struct sidlist * self, sid_t id );
int32_t sidlist_adds( struct sidlist * self, sid_t * ids, uint32_t count );
sid_t sidlist_del( struct sidlist * self, int32_t index );
void sidlist_destroy( struct sidlist * self );

// ��������
enum
{
	eTaskType_Null		= 0,	// ������
	eTaskType_User		= 1,	// �û�����
	eTaskType_Data		= 2,	// ��������
};

// ������䳤��
#define TASK_PADDING_SIZE		56	

// ��������
struct task
{
	int16_t type;				// 2bytes
	int16_t utype;				// 2bytes
	union
	{
		void *	taskdata;			 
		char	data[TASK_PADDING_SIZE];
	};
};

QUEUE_PADDING_HEAD(taskqueue, struct task);
QUEUE_PROTOTYPE(taskqueue, struct task)

/* 
 * ��Ϣ����
 * �̰߳�ȫ����Ϣ����, ��֪ͨ�Ĺ���
 */
struct msgqueue
{
	struct taskqueue queue;
	int32_t popfd;
	int32_t pushfd;

	pthread_mutex_t lock; 
};

// ������Ϣ����
struct msgqueue * msgqueue_create( uint32_t size );

// �����߷�������
// isnotify - �Ƿ���Ҫ֪ͨ������
int32_t msgqueue_push( struct msgqueue * self, struct task * task, uint8_t isnotify );

// �����ߴ���Ϣ������ȡһ����������
int32_t msgqueue_pop( struct msgqueue * self, struct task * task );

// ����
int32_t msgqueue_swap( struct msgqueue * self, struct taskqueue * queue );

// ��Ϣ���еĳ���
uint32_t msgqueue_count( struct msgqueue * self );

// �����߹ܵ�fd
int32_t msgqueue_popfd( struct msgqueue * self );

// ������Ϣ����
int32_t msgqueue_destroy( struct msgqueue * self );

#ifdef __cplusplus
}
#endif

#endif

