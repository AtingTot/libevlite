
#ifndef SRC_IOLAYER_H
#define SRC_IOLAYER_H

#include <stdint.h>

#include "event.h"
#include "threads.h"
#include "network.h"

#include "session.h"

struct iolayer
{
	// ��������
	uint8_t		nthreads;
	uint32_t	nclients;
	
	// �����߳���
	iothreads_t group;

	// �Ự������
	void **		managers;
};

// ������
struct acceptor
{
	int32_t 	fd;

	// �����¼�
	event_t		event;
	
	// �󶨵ĵ�ַ�Լ������Ķ˿ں�
	char		host[INET_ADDRSTRLEN];
	uint16_t	port;

	// �߼�
	void * 		context;
	int32_t 	(*cb)(void *, sid_t, const char *, uint16_t);

	// ͨ�Ų���
	struct iolayer * parent;
};

// ������
struct connector
{
	int32_t		fd;

	// �����¼�
	event_t		event;

	// ���ӷ������ĵ�ַ�Ͷ˿ں�
	char		host[INET_ADDRSTRLEN];
	uint16_t	port;

	// �߼�
	int32_t		seconds;
	void *		context;
	int32_t		(*cb)( void *, int32_t, const char *, uint16_t, sid_t);

	// ͨ�Ų���
	struct iolayer * parent;
};

//
// NOTICE: �����������󳤶Ȳ�����56
//

// NOTICE: task_assign�����Ѿ��ﵽ48bytes
struct task_assign
{
	int32_t		fd;							// 4bytes

	uint16_t	port;						// 2bytes
	char 		host[INET_ADDRSTRLEN];		// 16bytes + 2bytes

	void *		context;					// 8bytes
	int32_t		(*cb)(void *, sid_t, const char *, uint16_t);	// 8bytes
};

struct task_send
{
	sid_t						id;			// 8bytes
	char *						buf;		// 8bytes
	uint32_t					nbytes;		// 4bytes
	int32_t						isfree;		// 4bytes
};

// �������ڴ���벻��Ҫʹ����
#pragma pack(1)
#pragma pack()

// ����һ���Ự
struct session * iolayer_alloc_session( struct iolayer * self, int32_t key );

// ��������Զ�̷�����
int32_t iolayer_reconnect( struct iolayer * self, struct connector * connector );

// ���������� 
int32_t iolayer_free_connector( struct iolayer * self, struct connector * connector );

// ����ǰ�̷ַ߳�һ���Ự
int32_t iolayer_assign_session( struct iolayer * self, uint8_t index, struct task_assign * task );

#endif

