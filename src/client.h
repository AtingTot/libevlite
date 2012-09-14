
#ifndef SRC_CLIENT_H
#define SRC_CLIENT_H

#include "threads.h"
#include "session.h"

struct client
{
	// �����߳���, ֻ��1���߳�
	iothreads_t		group;

	struct session	session;
	
	// ������
	void *			connector;	
};

// ������
struct connector
{
	int32_t		fd;

	// �����¼�
	event_t		event;

	//
	evsets_t			sets;
	struct session *	session;

	// ���ӷ������ĵ�ַ�Ͷ˿ں�
	char		host[INET_ADDRSTRLEN];
	uint16_t	port;

	// �߼�
	int32_t		seconds;
	void *		context;
	int32_t		(*cb)( void *, int32_t );
};

//
// NOTICE: �����������󳤶Ȳ�����56
//

struct ctask_send
{
	char *				buf;		// 8bytes
	uint32_t			nbytes;		// 4bytes+4bytes
	struct session *	session;	// 8bytes
};

// �������ڴ���벻��Ҫʹ����
#pragma pack(1)
#pragma pack()

int32_t client_connect_direct( evsets_t sets, struct connector * connector );

#endif


