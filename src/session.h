
#ifndef SRC_SESSION_H
#define SRC_SESSION_H

/*
 * session �Ự
 * һ��TCP���ӵĻ�����Ԫ
 */

#include <stdint.h>
#include <netinet/in.h>

#include "event.h"
#include "network.h"

#include "utils.h"
#include "message.h"

#define SESSION_READING			0x01	// �ȴ����¼�
#define SESSION_WRITING			0x02	// �ȴ�д�¼�, [����, ����, ����]
#define SESSION_KEEPALIVING		0x04	// �ȴ������¼�
#define SESSION_EXITING			0x10	// �ȴ��˳�, ����ȫ��������Ϻ�, ������ֹ

enum SessionType
{
	eSessionType_Once 		= 1,	// ��ʱ�Ự
	eSessionType_Persist	= 2,	// ���ûỰ, �ж��������Ĺ���
};

struct session_setting
{
	int32_t timeout_msecs;
	int32_t keepalive_msecs;
	int32_t max_inbuffer_len;
};

QUEUE_HEAD( sendqueue, struct message * );
QUEUE_PROTOTYPE( sendqueue, struct message * )

struct session
{
	sid_t		id;

	int32_t 	fd;
	int8_t 		type;
	int8_t 		status;
	
	uint16_t	port;
	char 		host[INET_ADDRSTRLEN];

	// ��д�Լ���ʱ�¼�
	event_t 	evread;
	event_t 	evwrite;
	event_t		evkeepalive;

	// �¼����͹�����
	evsets_t	evsets;
	void *		manager;

	// �߼�
	void *		context;
	ioservice_t service;
	
	// ���ջ�����
	struct buffer		inbuffer;

	// ���Ͷ����Լ���Ϣƫ����
	int32_t 			msgoffsets;
	struct sendqueue 	sendqueue;

	// �Ự������
	struct session_setting setting;
};

// 64λSID�Ĺ���
// |XXXXXX	|XX		|XXXXXXXX	|
// |RES		|INDEX	|SEQ		|
// |24		|8		|32			|

#define SID_MASK	0x000000ffffffffffULL
#define SEQ_MASK	0x00000000ffffffffULL
#define INDEX_MASK	0x000000ff00000000ULL

#define SID_SEQ(sid)	( (sid)&SEQ_MASK )
#define SID_INDEX(sid)	( ( ((sid)&INDEX_MASK) >> 32 ) - 1 )

// �Ự��ʼ
int32_t session_start( struct session * self, int8_t type, int32_t fd, evsets_t sets );

// 
void session_set_endpoint( struct session * self, char * host, uint16_t port );

// ��session��������
int32_t session_send( struct session * self, char * buf, uint32_t nbytes );

// ��session�ķ��Ͷ��������һ����Ϣ
int32_t session_append( struct session * self, struct message * message );

// �Ựע��/��ע�������¼�
void session_add_event( struct session * self, int16_t ev );
void session_del_event( struct session * self, int16_t ev );

// ��ʼ��������
int32_t session_start_keepalive( struct session * self );

// ����Զ�̷�����
int32_t session_start_reconnect( struct session * self );

// ������ֹ�Ự
// ��API�ᾡ���ѷ��Ͷ����е����ݷ��ͳ�ȥ
// libevlite��ȫ��ֹ�Ự�ĺ���ģ��
int32_t session_shutdown( struct session * self );

// �Ự����
int32_t session_end( struct session * self, sid_t id );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct hashtable;
struct session_manager
{
	uint8_t		index;
	uint32_t	autoseq;		// ���������
	
	struct hashtable * table;	
};

// �����Ự������
// index	- �Ự������������
// count	- �Ự�������й�����ٸ��Ự
struct session_manager * session_manager_create( uint8_t index, uint32_t size );

// ����һ���Ự
struct session * session_manager_alloc( struct session_manager * self );

// �ӻỰ��������ȡ��һ���Ự
struct session * session_manager_get( struct session_manager * self, sid_t id );

// �ӻỰ���������Ƴ��Ự
int32_t session_manager_remove( struct session_manager * self, struct session * session );

// ���ٻỰ������
void session_manager_destroy( struct session_manager * self );

#endif

