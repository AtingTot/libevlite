
#include <assert.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "iolayer.h"
#include "channel.h"
#include "network-internal.h"

#include "session.h"

static struct session * _new_session( uint32_t size );
static int32_t _del_session( struct session * self );
static inline int32_t _send( struct session * self, char * buf, uint32_t nbytes );

struct session * _new_session( uint32_t size )
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
	if ( arraylist_init(&self->outmsglist, size) != 0 )
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
	
	// ���ٷ��Ͷ���
	arraylist_final( &self->outmsglist );

	// ���ٽ��ջ�����
	buffer_set( &self->inbuffer, NULL, 0 );

	free( self );

	return 0;
}

//
int32_t _send( struct session * self, char * buf, uint32_t nbytes )
{
	int32_t rc = 0;

	// �ȴ��رյ�����
	if ( self->status&SESSION_EXITING )
	{
		return -1;
	}

	// �ж�session�Ƿ�æ
	if ( !(self->status&SESSION_WRITING) 
			&& arraylist_count(&self->outmsglist) == 0 )
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
	arraylist_append( &self->outmsglist, message );
	session_add_event( self, EV_WRITE );

	return rc;
}

int32_t session_start( struct session * self, int8_t type, int32_t fd, evsets_t sets )
{
	self->fd		= fd;
	self->type		= type;
	self->evsets	= sets;

	// TODO: ����Ĭ�ϵ������ջ�����
	
	// ����Ҫÿ�ο�ʼ�Ự��ʱ���ʼ��
	// ֻ��Ҫ��manager�����Ự��ʱ���ʼ��һ�Σ�����	

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
			// ��Ϣ�Ѿ��ɹ�����
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
	
	// ���ݸ���
	char * buffer = service->transform( self->context, (const char *)buf, &nbytes );

	if ( buffer == buf )
	{
		// ��Ϣδ���и���

		// ��ӵ��Ự�ķ����б���
		rc = arraylist_append( &self->outmsglist, message );
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
			rc = 1;
		}

		free( buffer );
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
		evsets_del( sets, self->evread );
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

	// ��ֹ������
	if ( self->fd > 0 )
	{
		close( self->fd );
	}

	// ����Զ�̷�����
	self->fd = tcp_connect( self->host, self->port, 1 ); 
	if ( self->fd < 0 )
	{
		return channel_error( self, eIOError_ConnectFailure );
	}

	event_set( self->evwrite, self->fd, EV_WRITE );
	event_set_callback( self->evwrite, channel_on_reconnect, self );
	evsets_add( sets, self->evwrite, 0 );
	self->status |= SESSION_WRITING;

	return 0;
}

int32_t session_shutdown( struct session * self )
{
	if ( !(self->status&SESSION_EXITING)
		&& arraylist_count(&self->outmsglist) > 0 )
	{
		// �Ự״̬����, ���ҷ��Ͷ��в�Ϊ��
		// ���Լ�����δ���͵����ݷ��ͳ�ȥ, ����ֹ�Ự
		self->status |= SESSION_EXITING;

		// ɾ�����¼�
		session_del_event( self, EV_READ );
		// ע��д�¼�
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
	int32_t count = arraylist_count( &self->outmsglist );
	if ( count > 0 )
	{
		// �Ự��ֹʱ���Ͷ��в�Ϊ��
		syslog(LOG_WARNING, "%s(SID=%ld)'s Out-Message-List (%d) is not empty .", __FUNCTION__, id, count );

		for ( ; count >= 0; --count )
		{
			struct message * msg = arraylist_take( &self->outmsglist, -1 );
			if ( msg )
			{
				// ��Ϣ����ʧ��һ��
				message_add_failure( msg, id );

				// �����Ϣ�Ƿ����������
				if ( message_is_complete(msg) == 0 )
				{
					message_destroy( msg );
				}
			}
		}
	}
	
	// ��ս��ջ�����
	buffer_erase( &self->inbuffer, buffer_length(&self->inbuffer) );

	// ɾ���¼�
	if ( self->evread && (self->status&SESSION_READING) )
	{
		evsets_del( self->evsets, self->evread );
		self->status &= ~SESSION_READING;
	}
	if ( self->evwrite && (self->status&SESSION_WRITING) )
	{
		evsets_del( self->evsets, self->evwrite );
		self->status &= ~SESSION_WRITING;
	}
	if ( self->evkeepalive && (self->status&SESSION_KEEPALIVING) )
	{
		evsets_del( self->evsets, self->evkeepalive );
		self->status &= ~SESSION_KEEPALIVING;
	}

	// NOTICE: ���һ����ֹ������
	// ϵͳ�����������, libevlite��������������Ϊkey�ĻỰ
	if ( self->fd > 0 )
	{
		close( self->fd );
		self->fd = -1;
	}

	_del_session( self );

	return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// ��װ����
#define ENLARGE_FACTOR	0.5f

enum
{
	eBucketStatus_Free		= 0,
	eBucketStatus_Deleted	= 1,
	eBucketStatus_Occupied	= 2,
};

struct hashbucket
{
	int8_t status;
	struct session * session;
};

struct hashtable
{
	uint32_t size;
	uint32_t count;
	uint32_t enlarge_size;				// = size*ENLARGE_FACTOR
	
	struct hashbucket * buckets;
};

static inline int32_t _init_table( struct hashtable * table, uint32_t size );
static inline int32_t _enlarge_table( struct hashtable * table );
static inline int32_t _find_position( struct hashtable * table, sid_t id );
static inline int32_t _append_session( struct hashtable * table, struct session * s );
static inline int32_t _remove_session( struct hashtable * table, struct session * s );
static inline struct session * _find_session( struct hashtable * table, sid_t id );

int32_t _init_table( struct hashtable * table, uint32_t size )
{
	table->count = 0;
	table->size = size;
	table->enlarge_size = (uint32_t)( (float)size * ENLARGE_FACTOR );
	
	table->buckets = calloc( size, sizeof(struct hashbucket) );
	if ( table->buckets == NULL )
	{
		return -1;
	}

	return 0;
}

int32_t _enlarge_table( struct hashtable * table )
{
	uint32_t i = 0;
	struct hashtable newtable;	

	if ( _init_table(&newtable, table->size<<1) != 0 )
	{
		return -1;
	}

	for ( i = 0; i < table->size; ++i )
	{
		struct hashbucket * bucket = table->buckets + i;

		if ( bucket->session != NULL 
				&& bucket->status == eBucketStatus_Occupied )
		{
			_append_session( &newtable, bucket->session );
		}
	}

	assert( newtable.count == table->count );

	free ( table->buckets );
	table->buckets = newtable.buckets;
	table->size = newtable.size;
	table->enlarge_size = newtable.enlarge_size;

	return 0;
}

int32_t _find_position( struct hashtable * table, sid_t id )
{
	int32_t pos = -1; 
	int32_t probes = 0;
	int32_t bucknum = SID_SEQ(id) & (table->size-1);

	while ( 1 )
	{
		struct hashbucket * bucket = table->buckets + bucknum;

		if ( bucket->status == eBucketStatus_Free
				|| bucket->status == eBucketStatus_Deleted )
		{
			if ( pos == -1 )
			{
				pos = bucknum;
			}

			if ( bucket->status == eBucketStatus_Free )
			{
				return pos;
			}
		}	
		else if ( bucket->session->id == id )
		{
			return bucknum;
		}

		++probes;
		bucknum = (bucknum + probes) & (table->size-1);
	}

	return -1;
}

int32_t _append_session( struct hashtable * table, struct session * s )
{
	if ( table->count >= table->enlarge_size )
	{
		// ��չ
		if ( _enlarge_table( table ) != 0 )
		{
			return -1;
		}
	}

	int32_t pos = _find_position( table, s->id );
	if ( pos == -1 )
	{
		return -1;
	}

	struct hashbucket * bucket = table->buckets + pos;
	if ( bucket->session != NULL && bucket->session->id == s->id )
	{
		syslog(LOG_WARNING, "%s(Index=%d): the SID (Seq=%u, Sid=%ld) conflict !", __FUNCTION__, SID_INDEX(s->id), SID_SEQ(s->id), s->id );
		return -2;
	}
	
	++table->count;
	bucket->session = s;
	bucket->status = eBucketStatus_Occupied;

	return 0;
}

int32_t _remove_session( struct hashtable * table, struct session * s )
{
	int32_t pos = _find_position( table, s->id );
	if ( pos == -1 )
	{
		return -1;
	}

	if ( table->buckets[pos].status == eBucketStatus_Free
	  || table->buckets[pos].status == eBucketStatus_Deleted )
	{
		return -2;
	}

	struct hashbucket * bucket = table->buckets + pos;
	assert( bucket->session == s );
	
	--table->count;
	bucket->session = NULL;
	bucket->status = eBucketStatus_Deleted;

	return 0;	
}

struct session * _find_session( struct hashtable * table, sid_t id )
{
	int32_t pos = _find_position( table, id );
	if ( pos == -1 )
	{
		return NULL;
	}

	if ( table->buckets[pos].status == eBucketStatus_Free
	  || table->buckets[pos].status == eBucketStatus_Deleted )
	{
		return NULL;
	}

	assert( table->buckets[pos].session != NULL );
	assert( table->buckets[pos].session->id == id );

	return table->buckets[pos].session;
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

	self->seq = 0;
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
	id |= self->seq++;
	id &= SID_MASK;

	session = _new_session( OUTMSGLIST_DEFAULT_SIZE );
	if ( session != NULL )
	{
		session->id = id;
		session->manager = self;

		// ��ӻỰ
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
		syslog( LOG_WARNING, "%s(Index=%u): the number of residual active session is %d .", __FUNCTION__, self->index, self->table->count );
	}

	for ( i = 0; i < self->table->size; ++i )
	{
		struct hashbucket * bucket = self->table->buckets + i;

		if ( bucket->session != NULL 
				&& bucket->status == eBucketStatus_Occupied )
		{
			struct session * session = bucket->session; 

			if ( session )
			{
				session_end( session, session->id );
			}
		}
	}

	if ( self->table->buckets != NULL )
	{
		free( self->table->buckets );
		self->table->buckets = NULL;
	}

	free( self );
	return;
}

