
#ifndef SRC_UTILS_H
#define SRC_UTILS_H

/*
 * �����㷨ģ��
 */

#include <stdint.h>
#include <pthread.h>

//
// 
//
inline int64_t mtime();

//
// �����㷨��
//
uint32_t getpower( uint32_t size );
uint32_t nextpow2( uint32_t size );

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

	pthread_spinlock_t lock; 
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

