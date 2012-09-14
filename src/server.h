
#ifndef SRC_SERVER_H
#define SRC_SERVER_H

#include <stdint.h>

#include "event.h"
#include "threads.h"
#include "network.h"

#include "session.h"

struct server
{
	// ��������
	uint8_t		nthreads;
	uint32_t	nclients;
	
	// �����߳���
	iothreads_t group;

	// �Ự������
	void **		managers;
	
	// �Ự������
	void * 		acceptor;
};

// �Ự������
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

	// ���������
	struct server * parent;
};


//
// NOTICE: �����������󳤶Ȳ�����56
//

// NOTICE: stask_assign�����Ѿ��ﵽ48bytes
struct stask_assign
{
	int32_t		fd;							// 4bytes

	uint16_t	port;						// 2bytes
	char 		host[INET_ADDRSTRLEN];		// 16bytes + 2bytes

	void *		context;					// 8bytes
	int32_t		(*cb)(void *, sid_t, const char *, uint16_t);	// 8bytes
};

struct stask_send
{
	sid_t						id;			// 8bytes
	char *						buf;		// 8bytes
	uint32_t					nbytes;		// 4bytes+4bytes
};

// �������ڴ���벻��Ҫʹ����
#pragma pack(1)
#pragma pack()

// ����ǰ�̷ַ߳�һ���Ự
int32_t server_assign_session( struct server * self, uint8_t index, struct stask_assign * task );


#endif

