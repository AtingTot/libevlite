
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include "channel.h"
#include "network-internal.h"

#include "client.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static int32_t _send_direct( struct ctask_send * task );
static int32_t _connect_direct( evsets_t sets, struct connector * connector );
static void _io_methods( void * context, uint8_t index, int16_t type, void * task );
static int32_t _new_connector( struct client * self, 
				const char * host, uint16_t port, 
				int32_t seconds, int32_t (*cb)(void *, int32_t), void * context );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int32_t _new_connector( struct client * self, 
								const char * host, uint16_t port, 
								int32_t seconds, int32_t (*cb)(void *, int32_t), void * context )
{
	struct connector * connector = (struct connector *)self->connector;

	// ���������¼�
	connector->event = event_create();
	if ( connector->event == NULL )
	{
		return -1;
	}

	// ����Զ�̷�����
	connector->fd = tcp_connect( (char *)host, port, 0 );
	if ( connector->fd <= 0 )
	{
		return -2;
	}

	// ��������ʼ��
	connector->cb = cb;
	connector->context = context;
	connector->seconds = seconds;
	connector->sets = NULL;
	connector->port = port;
	connector->session = &self->session;
	strncpy( connector->host, host, INET_ADDRSTRLEN );

	return 0;
}

client_t client_start( const char * host, uint16_t port, 
						int32_t seconds, int32_t (*cb)(void *, int32_t), void * context )
{
	struct client * self = NULL;

	self = calloc( 1, sizeof(struct client)+sizeof(struct connector) );	
	if ( self == NULL )
	{
		return NULL;
	}

	self->connector = (struct connector *)( self+1 );
	
	// ��ʼ���Ự
	if ( session_init( &self->session, OUTMSGLIST_DEFAULT_SIZE*8  ) != 0 )
	{
		client_stop( self );
		return NULL;
	}
	
	// ���������߳���
	self->group = iothreads_create( 1, _io_methods, self );
	if ( self->group == NULL )
	{
		client_stop( self );
		return NULL;
	}
	
	// ����������
	if ( _new_connector( self, 
				host, port, seconds, cb, context ) == 0 )
	{
		client_stop( self );
		return NULL;
	}
	
	// �����߳��鿪ʼ����
	iothreads_start( self->group );
	iothreads_post( self->group, 0, eIOTaskType_Connect, self->connector, 0 );

	return self;
}

int32_t client_send( client_t self, const char * buf, uint32_t nbytes, int32_t iscopy )
{
	struct client * client = (struct client *)self;
	struct connector * connector = (struct connector *)client->connector;

	if ( iscopy )
	{
		char * buf2 = (char *)malloc( nbytes );
		if ( buf2 == NULL )
		{
			syslog(LOG_WARNING, "client_send(host:'%s', port:%d) failed, can't allocate the memory for 'buf2' .", connector->host, connector->port );
			return -2;
		}

		memcpy( buf2, buf, nbytes );
		buf = buf2;
	}

	struct ctask_send task;
	task.buf		= (char *)buf;
	task.nbytes		= nbytes;
	task.session	= &client->session;

	if ( pthread_self() == iothreads_get_id(client->group, 0) )
	{
		return _send_direct( &task );
	}

	return iothreads_post( client->group, 0, eIOTaskType_Send, (void *)&task, sizeof(task) );
}

int32_t client_set_keepalive( client_t self, int32_t seconds )
{
	// NOT Thread-Safe
	struct client * client = ( struct client * )self;

	client->session.setting.keepalive_msecs = seconds*1000;

	return 0;

}

int32_t client_set_service( client_t self, ioservice_t * service, void * context )
{
	// NOT Thread-Safe
	struct client * client = ( struct client * )self;

	client->session.context = context;
	client->session.service = *service;

	return 0;
}

// �ͻ���ֹͣ
int32_t client_stop( client_t self )
{
	struct client * client = (struct client *)self;
	struct connector * connector = (struct connector *)client->connector;

	// ֹͣ�����߳���
	if ( client->group )
	{
		iothreads_stop( client->group );
	}

	// ��ֹ�Ự
	session_end( &client->session, client->session.id );

	// ����������
	if ( connector )
	{
		// ���������¼�
		// �����߳���ֹͣʱ�¼������Ѿ�������

		if ( connector->event )
		{
			event_destroy( connector->event );
			connector->event = NULL;
		}

		if ( connector->fd > 0 )
		{
			close( connector->fd );
			connector->fd = -1;
		}

	}

	// ���������߳���
	if ( client->group )
	{
		iothreads_destroy( client->group );
		client->group = NULL;
	}

	// ���ٻỰ
	session_final( &client->session );

	free( client );
	return 0;
}

int32_t client_reconnect( struct connector * connector )
{
	// �ر�������
	if ( connector->fd > 0 )
	{
		close( connector->fd );
	}
	
	// ��������
	// �����Ƿ�������, ����ʱ����д�¼�
	connector->fd = tcp_connect( connector->host, connector->port, 0 );
	_connect_direct( connector->sets, connector );

	return 0;
}

int32_t _send_direct( struct ctask_send * task )
{
	return session_send( task->session, task->buf, task->nbytes );
}

int32_t _connect_direct( evsets_t sets, struct connector * connector )
{
	// ��ʼ��עconnector�¼�
	
	connector->sets = sets;
	
	event_set( connector->event, connector->fd, EV_WRITE|EV_PERSIST );
	event_set_callback( connector->event, channel_on_connect, connector );
	evsets_add( sets, connector->event, connector->seconds*1000 );
	
	return 0;
}

void _io_methods( void * context, uint8_t index, int16_t type, void * task )
{
	struct client * client = (struct client *)context;

	evsets_t sets = iothreads_get_sets( client->group, index );

	switch ( type )
	{
	
	case eIOTaskType_Connect:
		{
			//
			_connect_direct( sets, (struct connector *)task );
		}
		break;

	case eIOTaskType_Send :
		{
			// ��������
			_send_direct( (struct ctask_send *)task );
		}
		break;

	}

	return;
}

