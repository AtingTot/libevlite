
#include <assert.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"
#include "channel.h"
#include "network-internal.h"

#include "session.h"

int32_t session_init( struct session * self, uint32_t size )
{
	// ��ʼ�������¼�
	self->evread = event_create();
	if ( self->evread == NULL )
	{
		return -1;
	}
	self->evwrite = event_create();
	if ( self->evwrite == NULL )
	{
		return -2;
	}
	self->evkeepalive = event_create();
	if ( self->evkeepalive == NULL )
	{
		return -3;
	}
	
	// ��ʼ�����Ͷ���	
	if ( arraylist_init(&self->outmsglist, size) != 0 )
	{
		return -4;
	}

	return 0;
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
	session_add_event( self, EV_READ|EV_ET );
	session_start_keepalive( self );

	return 0;
}

void session_set_endpoint( struct session * self, char * host, uint16_t port )
{
	self->port = port;
	strncpy( self->host, host, INET_ADDRSTRLEN );
}

//
int32_t session_send( struct session * self, char * buf, uint32_t nbytes )
{
	// �ȴ��رյ�����

	if ( self->status&SESSION_EXITING )
	{
		return -3;
	}

	// �ж�session�Ƿ�æ
	if ( (self->status&SESSION_WRITING) 
		|| arraylist_count(&self->outmsglist) > 0 )
	{
		// ����message, ��ӵ����Ͷ�����
		struct message * message = message_create();
		if ( message == NULL )
		{
			free( buf );
			return -2;
		}

		message_add_receiver( message, self->id );
		message_set_buffer( message,  buf, nbytes );
		arraylist_append( &self->outmsglist, message );
		session_add_event( self, EV_WRITE|EV_ET );
		return 0;
	}

	// ֱ�ӷ���
	return channel_send( self, buf, nbytes ); 
}

//
int32_t session_append( struct session * self, struct message * message )
{
	int32_t rc = 0;

	// ��ӵ��Ự�ķ����б���
	rc = arraylist_append( &self->outmsglist, message );
	if ( rc == 0 )
	{
		// ע��д�¼�, �����Ͷ���
		session_add_event( self, EV_WRITE|EV_ET );
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
	self->fd = tcp_connect( self->host, self->port, 0 ); 
	if ( self->fd < 0 )
	{
		return channel_error( self, eIOError_ConnectFailure );
	}

	event_set( self->evwrite, self->fd, EV_WRITE|EV_ET );
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
		session_add_event( self, EV_WRITE|EV_ET );

		return 1;
	}

	return channel_shutdown( self );
}

int32_t session_end( struct session * self )
{
	// ��շ��Ͷ���
	int32_t count = arraylist_count( &self->outmsglist );
	if ( count > 0 )
	{
		// �Ự��ֹʱ���Ͷ��в�Ϊ��
		syslog(LOG_WARNING, "session_end(SID=%ld)'s Out-Message-List (%d) is not empty .", self->id, count );
	}
	for ( ; count > 0; --count )
	{
		struct message * msg = arraylist_take( &self->outmsglist, -1 );
		if ( msg )
		{
			// ��Ϣ����ʧ��һ��
			message_add_failure( msg, self->id );

			// �����Ϣ�Ƿ����������
			if ( message_is_complete(msg) == 0 )
			{
				message_destroy( msg );
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
	
	// ���ò�������
	self->id = 0;
	self->type = 0;
	self->status = 0;
	self->port = 0;
	self->host[0] = 0;
	self->evsets = NULL;
	self->context = NULL;
	self->service.error = NULL;
	self->service.timeout = NULL;
	self->service.process = NULL;
	self->service.shutdown = NULL;
	self->service.keepalive = NULL;
	self->msgoffsets = 0;
	self->setting.timeout_msecs = 0;
	self->setting.keepalive_msecs = 0;
	self->setting.max_inbuffer_len = 0;

	// NOTICE: ���һ����ֹ������
	// ϵͳ�����������, libevlite��������������Ϊkey�ĻỰ
	if ( self->fd > 0 )
	{
		close( self->fd );
		self->fd = -1;
	}

	return 0;
}

int32_t session_final( struct session * self )
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

	return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static struct session_entry * _get_entry( struct session_manager * self, sid_t id );

struct session_entry * _get_entry( struct session_manager * self, sid_t id )
{
	uint32_t i = 0;
	int32_t key = SID_KEY( id );
	
	struct arraylist * bucket = self->entries + ( key&(self->size-1) );
	for ( i = 0; i < arraylist_count(bucket); ++i )
	{
		struct session_entry * entry = arraylist_get( bucket, i );
		if ( entry->data.id == id )
		{
			// �Ựid�Ϸ�����entry�м�¼��key��seq�����
			// ���ִ������libevlite��˵���൱������

			assert( SID_KEY(id) == entry->key && SID_SEQ(id) == entry->seq );	
			return entry;
		}
	}

	return NULL;
}

struct session_manager * session_manager_create( uint8_t index, uint32_t size )
{
	uint32_t i = 0;
	struct session_manager * self = NULL;

	self = calloc( 1, sizeof(struct session_manager) );
	if ( self == NULL )
	{
		return NULL;
	}

	size = nextpow2( size );

	self->count = 0;
	self->size = size;
	self->index = index;

	self->entries = calloc( size, sizeof(struct arraylist) );
	if ( self->entries == NULL )
	{
		free( self );
		return NULL;
	}

	for ( i = 0; i < size; ++i )
	{
		int32_t rc = arraylist_init( self->entries+i, 8 );
		if ( rc != 0 )
		{
			uint32_t j = 0;
			for ( j = 0; j < i; ++j )
			{
				arraylist_final( self->entries+j );
			}

			free( self->entries );
			free( self );
			return NULL;
		}
	}

	return self;
}

struct session * session_manager_alloc( struct session_manager * self, int32_t key )
{
	sid_t id = 0;
	uint32_t i = 0;

	struct session * session = NULL;
	struct session_entry * entry = NULL; 
	struct arraylist * bucket = self->entries + ( key&(self->size-1) );

	for ( i = 0; i < arraylist_count(bucket); ++i )
	{
		entry = arraylist_get( bucket, i );
		if ( entry->key == key )
		{
			// ��ͬ������,���ִ�����������
			assert( entry->data.id == 0 );
			break;
		}
	}

	if ( i == arraylist_count(bucket) )
	{
		// û���ҵ����ʵĲ�λ

		// ����Ự���
		entry = calloc( 1, sizeof(struct session_entry) );
		if ( entry == NULL )
		{
			return NULL;
		}

		// ��ʼ���Ự
		if ( session_init( &entry->data, OUTMSGLIST_DEFAULT_SIZE ) != 0 )
		{
			// ��ʼ��������
			session_final( &entry->data );

			free( entry );
			return NULL;
		}
		entry->seq = 0;
		entry->key = key;

		// ��ӵ��б���
		arraylist_append( bucket, entry );
	}

	// ����sid
	id = self->index+1;
	id <<= 32;
	id |= (uint32_t)key;
	id <<= 16;
	id |= entry->seq;
	id &= SID_MASK;

	// �Ự��ʼ������
	session = &(entry->data);
	session->id = id;
	session->manager = self;

	return session;
}

struct session * session_manager_get( struct session_manager * self, sid_t id )
{
	struct session_entry * entry = _get_entry( self, id );
	if ( entry == NULL )
	{
		return NULL;
	}

	return &( entry->data );
}

int32_t session_manager_remove( struct session_manager * self, struct session * session )
{
	sid_t id = session->id;
	
	struct session_entry * entry = _get_entry( self, id );	
	if ( entry == NULL )
	{
		return -1;
	}

	// �������
	++entry->seq;
	
	// �Ự�������
	session->id = 0;
	session->manager = NULL;

	return 0;
}

void session_manager_destroy( struct session_manager * self )
{
	int32_t i = 0;
	int32_t nalive = 0;

	for ( i = 0; i < self->size; ++i )
	{
		struct arraylist * bucket = self->entries+i;
		
		while ( arraylist_count(bucket) > 0 )
		{
			struct session_entry * entry = arraylist_take( bucket, -1 );

			if ( entry )
			{
				if ( SID_KEY(entry->data.id) == entry->key
						&& SID_SEQ(entry->data.id) == entry->seq )
				{
					// �Ự���ڼ���״̬
					session_end( &entry->data );
					++nalive;
				}
				
				// ����	
				session_final( &entry->data );
				free( entry );
			}
		}

		arraylist_final( bucket );
	}

	if ( nalive > 0 )
	{
		syslog(LOG_WARNING, "session_manager_destroy(Index=%u): the number of residual active session is %d .", self->index, nalive );
	}

	if ( self->entries )
	{
		free( self->entries );
		self->entries = NULL;
	}

	free( self );
	return;
}

