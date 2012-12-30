
#include <stdio.h>
#include <assert.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "iolayer.h"
#include "channel.h"
#include "network-internal.h"

#include "session.h"

static struct session * _new_session();
static int32_t _del_session( struct session * self );
static inline void _stop( struct session * self );
static inline int32_t _send( struct session * self, char * buf, uint32_t nbytes );

//
QUEUE_GENERATE( sendqueue, struct message * )

//
struct session * _new_session()
{
	struct session * self = NULL;

	self = calloc( 1, sizeof(struct session) );
	if ( self == NULL )
	{
		return NULL;
	}

	// ��ʼ�������¼�
	self->evread = event_create();
	self->evwrite = event_create();
	self->evkeepalive = event_create();
	if ( self->evkeepalive == NULL 
			|| self->evread == NULL || self->evwrite == NULL )
	{
		_del_session( self );
		return NULL;
	}

	// ��ʼ�����Ͷ���
	if ( QUEUE_INIT(sendqueue)(&self->sendqueue, DEFAULT_SENDQUEUE_SIZE) != 0 )
	{
		_del_session( self );
		return NULL;
	}

	return self;
}

int32_t _del_session( struct session * self )
{
	// ���������¼�
	if ( self->evread )
	{
		event_destroy( self->evread );
		self->evread = NULL;
	}
	if ( self->evwrite )
	{
		event_destroy( self->evwrite );
		self->evwrite = NULL;
	}
	if ( self->evkeepalive )
	{
		event_destroy( self->evkeepalive );
		self->evkeepalive = NULL;
	}

	QUEUE_CLEAR(sendqueue)(&self->sendqueue);
	buffer_set( &self->inbuffer, NULL, 0 );
	free( self );

	return 0;
}

// �Ựֹͣ(ɾ�������¼��Լ��ر�������)
void _stop( struct session * self )
{
	evsets_t sets = self->evsets;

	// ɾ�������¼�
	if ( self->status&SESSION_READING )
	{
		evsets_del( sets, self->evread );
		self->status &= ~SESSION_READING;
	}
	if ( self->status&SESSION_WRITING )
	{
		evsets_del( sets, self->evwrite );
		self->status &= ~SESSION_WRITING;
	}
	if ( self->status&SESSION_KEEPALIVING )
	{
		evsets_del( sets, self->evkeepalive );
		self->status &= ~SESSION_KEEPALIVING;
	}

	// ��ս��ջ�����
	buffer_erase( &self->inbuffer, buffer_length(&self->inbuffer) );

	// �ر�������
	if ( self->fd > 0 )
	{
		close( self->fd );
		self->fd = -1;
	}
}

//
int32_t _send( struct session * self, char * buf, uint32_t nbytes )
{
	int32_t rc = 0;

	if ( self->status&SESSION_EXITING )
	{
		// �ȴ��رյ�����
		return -1;
	}

	// �ж�session�Ƿ�æ
	if ( !(self->status&SESSION_WRITING) 
			&& session_sendqueue_count(self) == 0 )
	{
		// ֱ�ӷ���
		rc = channel_send( self, buf, nbytes ); 
		if ( rc == nbytes )
		{
			// ȫ�����ͳ�ȥ
			return rc;
		}

		// Ϊʲô���ʹ���û��ֱ����ֹ�Ự�أ�
		// �ýӿ��п�����ioservice_t�е���, ֱ����ֹ�Ự��, �����������ԻỰ�Ĳ�������
	}

	// ����message, ��ӵ����Ͷ�����
	struct message * message = message_create();
	if ( message == NULL )
	{
		return -2;
	}

	message_add_buffer( message, buf+rc, nbytes-rc );
	message_add_receiver( message, self->id );
	QUEUE_PUSH(sendqueue)(&self->sendqueue, &message);
	session_add_event( self, EV_WRITE );

	return rc;
}

int32_t session_start( struct session * self, int8_t type, int32_t fd, evsets_t sets )
{
	assert( self->service.start != NULL );
	assert( self->service.process != NULL );
	assert( self->service.transform != NULL );
	assert( self->service.keepalive != NULL );
	assert( self->service.timeout != NULL );
	assert( self->service.error != NULL );
	assert( self->service.shutdown != NULL );

	self->fd		= fd;
	self->type		= type;
	self->evsets	= sets;

	// TODO: ����Ĭ�ϵ������ջ�����

	// ����Ҫÿ�ο�ʼ�Ự��ʱ���ʼ��
	// ֻ��Ҫ��manager�����Ự��ʱ���ʼ��һ�Σ�����	

	self->service.start( self->context );

	// ��ע���¼�, ���迪����������
	session_add_event( self, EV_READ );
	session_start_keepalive( self );

	return 0;
}

void session_set_endpoint( struct session * self, char * host, uint16_t port )
{
	self->port = port;
	strncpy( self->host, host, INET_ADDRSTRLEN );
}

int32_t session_send( struct session * self, char * buf, uint32_t nbytes )
{
	int32_t rc = -1;
	ioservice_t * service = &self->service;

	// ���ݸ���(���� or ѹ��)
	uint32_t _nbytes = nbytes;
	char * _buf = service->transform( self->context, (const char *)buf, &_nbytes );

	if ( _buf != NULL )
	{
		// ��������
		// TODO: _send()���Ը��ݾ�����������Ƿ�copy�ڴ�
		rc = _send( self, _buf, _nbytes );

		if ( _buf != buf )
		{
			// ���ٸ������Ϣ
			free( _buf );
		}
	}

	return rc;
}

//
int32_t session_append( struct session * self, struct message * message )
{
	int32_t rc = -1;
	ioservice_t * service = &self->service;

	char * buf = message_get_buffer( message );
	uint32_t nbytes = message_get_length( message );

	// ���ݸ���(���� or ѹ��)
	char * buffer = service->transform( self->context, (const char *)buf, &nbytes );

	if ( buffer == buf )
	{
		// ��Ϣδ���и���

		// ��ӵ��Ự�ķ����б���
		rc = QUEUE_PUSH(sendqueue)(&self->sendqueue, &message);
		if ( rc == 0 )
		{
			// ע��д�¼�, �����Ͷ���
			session_add_event( self, EV_WRITE );
		}
	}
	else if ( buffer != NULL )
	{
		// ��Ϣ����ɹ�

		rc = _send( self, buffer, nbytes );
		if ( rc >= 0 )
		{
			// ��������Ϣ�Ѿ���������
			message_add_success( message );
		}

		free( buffer );
	}

	if ( rc < 0 )
	{
		message_add_failure( message, self->id );
	}

	return rc;
}

// ע�������¼� 
void session_add_event( struct session * self, int16_t ev )
{
	int8_t status = self->status;
	evsets_t sets = self->evsets;

	// ע����¼�
	// ���ڵȴ����¼��������Ự
	if ( !(status&SESSION_EXITING)
			&& (ev&EV_READ) && !(status&SESSION_READING) )
	{
		event_set( self->evread, self->fd, ev );
		event_set_callback( self->evread, channel_on_read, self );
		evsets_add( sets, self->evread, self->setting.timeout_msecs );

		self->status |= SESSION_READING;
	}

	// ע��д�¼�
	if ( (ev&EV_WRITE) && !(status&SESSION_WRITING) )
	{
		int32_t wait_for_shutdown = 0;

		// �ڵȴ��˳��ĻỰ�����ǻ����10s�Ķ�ʱ��
		if ( status&SESSION_EXITING )
		{
			// �Զ��������� + Socket���������������
			// ��һֱ�Ȳ���EV_WRITE����, ��ʱlibevlite�ͳ����˻Ựй©
			wait_for_shutdown = MAX_SECONDS_WAIT_FOR_SHUTDOWN;
		}

		event_set( self->evwrite, self->fd, ev );
		event_set_callback( self->evwrite, channel_on_write, self );
		evsets_add( sets, self->evwrite, wait_for_shutdown );

		self->status |= SESSION_WRITING;
	}

	return;
}

// ��ע�������¼�
void session_del_event( struct session * self, int16_t ev )
{
	int8_t status = self->status;
	evsets_t sets = self->evsets;

	if ( (ev&EV_READ) && (status&SESSION_READING) )
	{
		evsets_del( sets, self->evread );
		self->status &= ~SESSION_READING;
	}

	if ( (ev&EV_WRITE) && (status&SESSION_WRITING) )
	{
		evsets_del( sets, self->evwrite );
		self->status &= ~SESSION_WRITING;
	}

	return;
}

int32_t session_start_keepalive( struct session * self )
{
	int8_t status = self->status;
	evsets_t sets = self->evsets;

	if ( self->setting.keepalive_msecs > 0 && !(status&SESSION_KEEPALIVING) )
	{
		event_set( self->evkeepalive, -1, 0 );
		event_set_callback( self->evkeepalive, channel_on_keepalive, self );
		evsets_add( sets, self->evkeepalive, self->setting.keepalive_msecs );

		self->status |= SESSION_KEEPALIVING;
	}	

	return 0;
}

int32_t session_start_reconnect( struct session * self )
{
	evsets_t sets = self->evsets;

	if ( self->status&SESSION_EXITING )
	{
		// �Ự�ȴ��˳�,
		return -1;
	}

	if ( self->status&SESSION_WRITING )
	{
		// ���ڵȴ�д�¼��������
		return -2;
	}

	// ֹͣ�Ự
	_stop( self );

	// 2���������, ����æ��
	event_set( self->evwrite, -1, 0 );
	event_set_callback( self->evwrite, channel_on_reconnect, self );
	evsets_add( sets, self->evwrite, TRY_RECONNECT_INTERVAL );
	self->status |= SESSION_WRITING;			// ��sessionæ����

	return 0;
}

int32_t session_shutdown( struct session * self )
{
	if ( !(self->status&SESSION_EXITING)
			&& session_sendqueue_count(self) > 0 )
	{
		// �Ự״̬����, ���ҷ��Ͷ��в�Ϊ��
		// ���Լ�����δ���͵����ݷ��ͳ�ȥ, ����ֹ�Ự
		self->status |= SESSION_EXITING;

		// ���Ȱ����ݷ���ȥ
		session_del_event( self, EV_READ );
		session_add_event( self, EV_WRITE );

		return 1;
	}

	return channel_shutdown( self );
}

int32_t session_end( struct session * self, sid_t id )
{
	// ���ڻỰ�Ѿ��ӹ�������ɾ����
	// �Ự�е�ID�Ѿ��Ƿ�

	// ��շ��Ͷ���
	uint32_t count = session_sendqueue_count(self);
	if ( count > 0 )
	{
		syslog(LOG_WARNING, 
				"%s(SID=%ld)'s Out-Message-List (%d) is not empty .", __FUNCTION__, id, count );

		for ( ; count > 0; --count )
		{
			struct message * msg = NULL;
			QUEUE_POP(sendqueue)( &self->sendqueue, &msg );

			// �����Ϣ�Ƿ����������
			message_add_failure( msg, id );
			if ( message_is_complete(msg) )
			{
				message_destroy( msg );
			}
		}
	}

	// ֹͣ�Ự
	_stop( self );
	_del_session( self );

	return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct hashnode
{
	struct hashnode * next;
	struct session * session;
};

struct hashtable
{
	uint32_t size;
	uint32_t count;
	struct hashnode * entries;
};

static inline int32_t _init_table( struct hashtable * table, uint32_t size );
static inline struct hashnode * _find_table( struct hashtable * table, sid_t id, int32_t flag );
static inline int32_t _append_session( struct hashtable * table, struct session * s );
static inline struct session * _find_session( struct hashtable * table, sid_t id );
static inline int32_t _remove_session( struct hashtable * table, struct session * s );

int32_t _init_table( struct hashtable * table, uint32_t size )
{
	assert( (size&(size-1)) == 0 );

	table->count = 0;
	table->size = size;

	table->entries = calloc( size, sizeof(struct hashnode) );
	if ( table->entries == NULL )
	{
		return -1;
	}

	return 0;
}

struct hashnode * _find_table( struct hashtable * table, sid_t id, int32_t flag )
{
	int32_t bucket = SID_SEQ(id) & (table->size-1);

	struct hashnode * node = NULL;
	struct hashnode * entries = table->entries + bucket;

	while ( 1 )
	{
		if ( entries->session != NULL )
		{
			if ( entries->session->id == id )
			{
				node = entries;
				break;
			}
		}
		else if ( node == NULL )
		{
			node = entries;
		}

		if ( entries->next == NULL )
		{
			break;
		}
		
		entries = entries->next;
	}

	if ( node == NULL && flag != 0 )
	{
		node = malloc( sizeof(struct hashnode) );

		if ( node != NULL )
		{
			node->next = NULL;
			node->session = NULL;
			entries->next = node;
		}
	}
	
	return node;
}

int32_t _append_session( struct hashtable * table, struct session * s )
{
	struct hashnode * node = _find_table( table, s->id, 1 );

	if ( node == NULL )
	{
		return -1;
	}

	if ( node->session != NULL && node->session->id == s->id )
	{
		syslog(LOG_WARNING, "%s(Index=%d): the SID (Seq=%u, Sid=%ld) conflict !", __FUNCTION__, SID_INDEX(s->id), SID_SEQ(s->id), s->id );
		return -2;
	}

	++table->count;
	node->session = s;
	
	return 0;
}

struct session * _find_session( struct hashtable * table, sid_t id )
{
	struct hashnode * node = _find_table( table, id, 0 );

	if ( node == NULL )
	{
		return NULL;
	}
	if ( node->session == NULL )
	{
		return NULL;
	}

	assert( node->session->id == id );
	return node->session;
}

int32_t _remove_session( struct hashtable * table, struct session * s )
{
	struct hashnode * node = _find_table( table, s->id, 0 );

	if ( node == NULL )
	{
		return -1;
	}
	if ( node->session == NULL )
	{
		return -2;
	}

	assert( node->session == s );
	
	--table->count;
	node->session = NULL;
	
	return 0;	
}

struct session_manager * session_manager_create( uint8_t index, uint32_t size )
{
	struct session_manager * self = NULL;

	self = calloc( 1, sizeof(struct session_manager)+sizeof(struct hashtable) );
	if ( self == NULL )
	{
		return NULL;
	}

	size = nextpow2( size );

	self->autoseq = 0;
	self->index = index;
	self->table = (struct hashtable *)( self + 1 );

	if ( _init_table(self->table, size) != 0 )
	{
		free( self );
		self = NULL;
	}		

	return self;
}

struct session * session_manager_alloc( struct session_manager * self )
{
	sid_t id = 0;
	struct session * session = NULL;

	// ����sid
	id = self->index+1;
	id <<= 32;
	id |= self->autoseq++;
	id &= SID_MASK;

	session = _new_session();
	if ( session != NULL )
	{
		session->id = id;
		session->manager = self;

		// ��ӻỰ
		// TODO: �Ƿ���Բ��, generate() and session_start()
		if ( _append_session(self->table, session) != 0 )
		{
			_del_session( session );
			session = NULL;
		}
	}	

	return session;
}

struct session * session_manager_get( struct session_manager * self, sid_t id )
{
	return _find_session( self->table, id );
}

int32_t session_manager_remove( struct session_manager * self, struct session * session )
{
	if ( _remove_session(self->table, session) != 0 )
	{
		return -1;
	}

	// �Ự�������
	session->id = 0;
	session->manager = NULL;

	return 0;
}

void session_manager_destroy( struct session_manager * self )
{
	int32_t i = 0;

	if ( self->table->count > 0 )
	{
		syslog( LOG_WARNING, 
				"%s(Index=%u): the number of residual active session is %d .", __FUNCTION__, self->index, self->table->count );
	}

	for ( i = 0; i < self->table->size; ++i )
	{
		int32_t nlist = 0;
		struct hashnode * node = self->table->entries + i;

		while ( node )
		{
			struct session * s = node->session;
			if ( s != NULL )
			{
				session_end( s, s->id );
				node->session = NULL;
			}

			struct hashnode * _node = node;
			node = node->next;
			if ( nlist++ != 0 )
			{
				free( _node );
			}
		}
	}

	if ( self->table->entries != NULL )
	{
		free( self->table->entries );
		self->table->entries = NULL;
	}

	free( self );
	return;
}

