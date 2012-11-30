
#ifndef SRC_UTILS_H
#define SRC_UTILS_H

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

#include "network.h"

//
// ϵͳ��صĲ���
//

// ʱ�亯��, ���غ�����
int64_t mtime();

// socket��������
int32_t is_connected( int32_t fd );
int32_t set_non_block( int32_t fd );
int32_t tcp_connect( char * host, uint16_t port, int8_t isasyn );
int32_t tcp_accept( int32_t fd, char * remotehost, uint16_t * remoteport );
int32_t tcp_listen( char * host, uint16_t port, void (*options)(int32_t) );

//
// �����㷨��
//
uint32_t getpower( uint32_t size );
uint32_t nextpow2( uint32_t size );

/*
 *
 */ 
struct arraylist
{
    uint32_t count;
    uint32_t size;

    void ** entries;
};

struct arraylist * arraylist_create( uint32_t size );
int32_t arraylist_init( struct arraylist * self, uint32_t size );
uint32_t arraylist_count( struct arraylist * self );
void arraylist_reset( struct arraylist * self );
void arraylist_final( struct arraylist * self );
int32_t arraylist_append( struct arraylist * self, void * data );
void * arraylist_get( struct arraylist * self, int32_t index );
void * arraylist_take( struct arraylist * self, int32_t index );
int32_t arraylist_destroy( struct arraylist * self );

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
		void *	task;			 
		char	data[TASK_PADDING_SIZE];
	};
};

/*
 * ����
 * �򵥵Ķ����㷨, ��Ҫ��չʱ, ��һ���������ݿ���
 * ����, �ڳ��ȿɿص������, �㹻��Ч�ͼ�
 */
struct queue
{
	uint32_t size;					// (4+4)bytes
	struct task * entries;			// 8bytes
	uint32_t padding1[12];			// 48,padding

	uint32_t head;					// 4bytes, cacheline padding
	uint32_t padding2[15];			// 60bytes 

	uint32_t tail;					// 4bytes, cacheline padding
	uint32_t padding3[15];			// 60bytes 
};

// TODO: �ֶδ洢����չʱ��ʡ���ڴ濽��

// ��������
// size - ���ܷ���Ŀ���, ȷ��size�㹻��
struct queue * queue_create( uint32_t size );

// ��������ύ����
int32_t queue_push( struct queue * self, struct task * task );

// �Ӷ�����ȡ��һ����������
int32_t queue_pop( struct queue * self, struct task * task );
int32_t queue_pops( struct queue * self, struct task * tasks, uint32_t count );

// ���г���
inline uint32_t queue_count( struct queue * self );

// ���ٶ���
int32_t queue_destroy( struct queue * self );

/* 
 * ��Ϣ����
 * �̰߳�ȫ����Ϣ����, ��֪ͨ�Ĺ���
 */
struct msgqueue
{
	struct queue * queue;

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
int32_t msgqueue_pops( struct msgqueue * self, struct task * tasks, uint32_t count );

// ��Ϣ���еĳ���
uint32_t msgqueue_count( struct msgqueue * self );

// �����߹ܵ�fd
int32_t msgqueue_popfd( struct msgqueue * self );

// ������Ϣ����
int32_t msgqueue_destroy( struct msgqueue * self );

#endif

