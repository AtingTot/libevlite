
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "network.h"

#include "channel.h"
#include "session.h"
#include "network-internal.h"

#include "server.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static int32_t _new_managers( struct server * self );
static int32_t _new_acceptor( struct server * self, 
							const char * host, uint16_t port, 
							int32_t (*cb)(void *, sid_t, const char *, uint16_t), void * context );

static int32_t _start_direct( evsets_t sets, struct acceptor * acceptor );
static int32_t _assign_direct( struct session_manager * manager, evsets_t sets, struct stask_assign * task );
static int32_t _send_direct( struct session_manager * manager, struct stask_send * task );
static int32_t _broadcast_direct( struct session_manager * manager, struct message * msg );
static int32_t _shutdown_direct( struct session_manager * manager, sid_t id );
static int32_t _shutdowns_direct( struct session_manager * manager, struct sidlist * ids );

static void _socket_option( int32_t fd );
static void _io_methods( void * context, uint8_t index, int16_t type, void * task );

static inline struct session_manager * _get_manager( struct server * self, uint8_t index );
static inline void _dispatch_sidlist( struct server * self, struct sidlist ** listgroup, sid_t * ids, uint32_t count );
static inline int32_t _send_buffer( struct server * self, sid_t id, const char * buf, uint32_t nbytes, int32_t iscopy );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int32_t _new_managers( struct server * self )
{
	uint8_t i = 0;
	uint32_t sessions_per_thread = self->nclients/self->nthreads; 

	// �Ự������, 
	// ����cacheline��������߷����ٶ�
	self->managers = calloc( (self->nthreads)<<3, sizeof(void *) );
	if ( self->managers == NULL )
	{
		return -1;
	}
	for ( i = 0; i < self->nthreads; ++i )
	{
		uint32_t index = i<<3;

		self->managers[index] = session_manager_create( i, sessions_per_thread );
		if ( self->managers[index] == NULL )
		{
			return -2;
		}
	}

	return 0;
}

int32_t _new_acceptor( struct server * self, 
							const char * host, uint16_t port, 
							int32_t (*cb)(void *, sid_t, const char *, uint16_t), void * context )
{
	struct acceptor * acceptor = (struct acceptor *)self->acceptor;

	// ���������¼�
	acceptor->event = event_create();
	if ( acceptor->event == NULL )
	{
		return -1;
	}

	// ����listenfd
	acceptor->fd = tcp_listen( (char *)host, port, _socket_option );
	if ( acceptor->fd <= 0 )
	{
		return -2;
	}

	// ��������ʼ��
	acceptor->cb = cb;
	acceptor->context = context;
	acceptor->parent = self;
	acceptor->port = port;
	strncpy( acceptor->host, host, INET_ADDRSTRLEN );

	return 0;
}

// ����������
server_t server_start( const char * host, uint16_t port,
							uint8_t nthreads, uint32_t nclients, 
							int32_t (*cb)(void *, sid_t, const char *, uint16_t), void * context )
{
	int32_t rc = 0;
	uint8_t index = 0;

	struct server * self = NULL;
	self = (struct server *)calloc( 1, sizeof(struct server)+sizeof(struct acceptor) );
	if ( self == NULL )
	{
		return NULL;
	}

	self->nthreads = nthreads;
	self->nclients = nclients;
	self->acceptor = (void *)(self + 1);

	// ��ʼ���Ự������
	rc = _new_managers( self );
	if ( rc != 0 )
	{
		server_stop( self );
		return NULL;
	}

	// �����Ự������
	rc = _new_acceptor( self, host, port, cb, context );
	if ( rc != 0 )
	{
		server_stop( self );
		return NULL;
	}

	// ���������߳���
	self->group = iothreads_create( self->nthreads, _io_methods, self );
	if ( self->group == NULL )
	{
		server_stop( self );
		return NULL;
	}

	// ����������
	iothreads_start( self->group );

	// ����ύһ�������̸߳������
	index = time(NULL) % (self->nthreads);
	iothreads_post( self->group, index, eIOTaskType_Listen, self->acceptor, 0 );

	return self;
}

void server_stop( server_t self )
{
	struct server * server = (struct server *)self;
	struct acceptor * acceptor = (struct acceptor *)server->acceptor;

	// ֹͣ�����߳���
	if ( server->group )
	{
		iothreads_stop( server->group );
	}

	// ���ٷ�����������
	if ( acceptor )
	{
		// ���ٽ����¼�
		// �����߳���ֹͣʱ�¼������Ѿ�������
		if ( acceptor->event )
		{
			event_destroy( acceptor->event );
			acceptor->event = NULL;
		}
		
		// �ر�������
		if ( acceptor->fd > 0 )
		{
			close( acceptor->fd );
			acceptor->fd = 0;
		}
	}

	// ���ٹ�����
	if ( server->managers )
	{
		uint8_t i = 0;
		for ( i = 0; i < server->nthreads; ++i )
		{
			struct session_manager * manager = server->managers[i<<3];	
			if ( manager )
			{
				session_manager_destroy( manager );
			}
		}
		free( server->managers );
		server->managers = NULL;
	}

	// ���������߳���
	if ( server->group )
	{
		iothreads_destroy( server->group );
		server->group = NULL;
	}

	free( server );

	return; 
}

int32_t server_set_timeout( server_t self, sid_t id, int32_t seconds )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct server * server = (struct server *)self;

	if ( index >= server->nthreads )
	{
		syslog(LOG_WARNING, "server_set_timeout(SID=%ld) failed, the Session's index[%u] is invalid .", id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( server, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "server_set_timeout(SID=%ld) failed, the Session's manager[%u] is invalid .", id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "server_set_timeout(SID=%ld) failed, the Session is invalid .", id );
		return -3;
	}

	session->setting.timeout_msecs = seconds*1000;

	return 0;
}

int32_t server_set_keepalive( server_t self, sid_t id, int32_t seconds )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct server * server = (struct server *)self;

	if ( index >= server->nthreads )
	{
		syslog(LOG_WARNING, "server_set_keepalive(SID=%ld) failed, the Session's index[%u] is invalid .", id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( server, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "server_set_keepalive(SID=%ld) failed, the Session's manager[%u] is invalid .", id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "server_set_keepalive(SID=%ld) failed, the Session is invalid .", id );
		return -3;
	}

	session->setting.keepalive_msecs = seconds*1000;

	return 0;
}

int32_t server_set_service( server_t self, sid_t id, ioservice_t * service, void * context )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct server * server = (struct server *)self;

	if ( index >= server->nthreads )
	{
		syslog(LOG_WARNING, "server_set_service(SID=%ld) failed, the Session's index[%u] is invalid .", id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( server, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "server_set_service(SID=%ld) failed, the Session's manager[%u] is invalid .", id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "server_set_service(SID=%ld) failed, the Session is invalid .", id );
		return -3;
	}

	session->context = context;
	session->service = *service;

	return 0;
}

int32_t server_send( server_t self, sid_t id, const char * buf, uint32_t nbytes, int32_t iscopy )
{
	return _send_buffer( (struct server *)self, id, buf, nbytes, iscopy );
}

int32_t server_broadcast( server_t self, sid_t * ids, uint32_t count, const char * buf, uint32_t nbytes, int32_t iscopy )
{
	// ��Ҫ����ids
	uint8_t i = 0;
	int32_t rc = 0;
	pthread_t threadid = pthread_self();

	struct sidlist * listgroup[ 256 ] = {NULL};
	struct server * server = (struct server *)self;

	_dispatch_sidlist( server, listgroup, ids, count );

	for ( i = 0; i < server->nthreads; ++i )
	{
		if ( listgroup[i] == NULL )
		{
			continue;
		}

		if ( sidlist_count( listgroup[i] ) > 1 )
		{
			struct message * msg = message_create();
			if ( msg == NULL )
			{
				continue;
			}

			message_add_buffer( msg, (char *)buf, nbytes );
			message_set_receivers( msg, listgroup[i] );

			if ( threadid == iothreads_get_id( server->group, i ) )
			{
				// ���߳���ֱ�ӹ㲥
				_broadcast_direct( _get_manager(server, i), msg );
			}
			else
			{
				// ���߳��ύ�㲥����
				iothreads_post( server->group, i, eIOTaskType_Broadcast, msg, 0 );
			}

			rc += sidlist_count( listgroup[i] );
			// listgroup[i] ����message������
		}
		else
		{
			sid_t id = sidlist_get( listgroup[i], 0 );

			if ( _send_buffer( server, id, buf, nbytes, iscopy ) == 0 )
			{
				// ���ͳɹ�
				rc += 1;
			}

			// ����listgroup[i]
			sidlist_destroy( listgroup[i] );
		}
	}

	return rc;
}

int32_t server_shutdown( server_t self, sid_t id )
{
	uint8_t index = SID_INDEX(id);
	struct server * server = (struct server *)self;

	if ( index >= server->nthreads )
	{
		syslog(LOG_WARNING, "server_shutdown(SID=%ld) failed, the Session's index[%u] is invalid .", id, index );
		return -1;
	}
	
	// �����ڻص�������ֱ����ֹ�Ự
	// ���������������ԻỰ�Ĳ������Ƿ���
#if 0
	if ( pthread_self() == iothreads_get_id( server->group, index ) )
	{
		// ���߳���ֱ����ֹ
		return _shutdown_direct( _get_manager(server, index), &task );
	}
#endif

	// ���߳��ύ��ֹ����
	return iothreads_post( server->group, index, eIOTaskType_Shutdown, (void *)&id, sizeof(id) );

}

int32_t server_shutdowns( server_t self, sid_t * ids, uint32_t count )
{
	// ��Ҫ����ids
	uint8_t i = 0;
	int32_t rc = 0;

	struct sidlist * listgroup[ 256 ] = {NULL};
	struct server * server = (struct server *)self;

	_dispatch_sidlist( server, listgroup, ids, count );

	for ( i = 0; i < server->nthreads; ++i )
	{
		if ( listgroup[i] == NULL )
		{
			continue;
		}

		// ����server_shutdown()

		// ���߳��ύ������ֹ����
		rc += sidlist_count( listgroup[i] );
		iothreads_post( server->group, i, eIOTaskType_Shutdowns, listgroup[i], 0 );
	}

	return rc;
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int32_t server_assign_session( struct server * self, uint8_t index, struct stask_assign * task )
{
	evsets_t sets = iothreads_get_sets( self->group, index );
	pthread_t threadid = iothreads_get_id( self->group, index );

	if ( pthread_self() == threadid )
	{
		// �ûỰ�ַ������߳�����
		return _assign_direct( _get_manager(self, index), sets, task );
	}

	// ���߳��ύ��������
	return iothreads_post( self->group, index, eIOTaskType_Assign, task, sizeof(struct stask_assign) );
}
 

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct session_manager * _get_manager( struct server * self, uint8_t index )
{
	if ( index >= self->nthreads )
	{
		return NULL;
	}

	return (struct session_manager *)( self->managers[index<<3] );
}

void _dispatch_sidlist( struct server * self, struct sidlist ** listgroup, sid_t * ids, uint32_t count )
{
	uint32_t i = 0;

	for ( i = 0; i < count; ++i )
	{
		uint8_t index = SID_INDEX( ids[i] );

		// index�Ƿ�
		if ( index >= self->nthreads )
		{
			continue;
		}

		// listδ����
		if ( listgroup[index] == NULL )
		{
			listgroup[index] = sidlist_create( count );
		}

		// ��ӵ�list��
		if ( listgroup[index] )
		{
			sidlist_add( listgroup[index], ids[i] );
		}
	}

	return;
}

int32_t _send_buffer( struct server * self, sid_t id, const char * buf, uint32_t nbytes, int32_t iscopy )
{
	uint8_t index = SID_INDEX(id);

	if ( index >= self->nthreads )
	{
		syslog(LOG_WARNING, "_send_buffer(SID=%ld) failed, the Session's index[%u] is invalid .", id, index );
		return -1;
	}

	if ( iscopy )
	{
		char * buf2 = (char *)malloc( nbytes );
		if ( buf2 == NULL )
		{
			syslog(LOG_WARNING, "_send_buffer(SID=%ld) failed, can't allocate the memory for 'buf2' .", id );
			return -2;
		}

		memcpy( buf2, buf, nbytes );
		buf = buf2;
	}

	struct stask_send task;
	task.id		= id;
	task.buf	= (char *)buf;
	task.nbytes	= nbytes;

	if ( pthread_self() == iothreads_get_id( self->group, index ) )
	{
		// ���߳���ֱ�ӷ���
		return _send_direct( _get_manager(self, index), &task );
	}

	// ���߳��ύ��������
	return iothreads_post( self->group, index, eIOTaskType_Send, (void *)&task, sizeof(task) );
}

void _socket_option( int32_t fd )
{
	int32_t flag = 0;
	struct linger ling;
	
	// Socket������
	set_non_block( fd );
	
	flag = 1;
	setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(flag) );
	
	flag = 1;
	setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flag, sizeof(flag) );

#if SAFE_SHUTDOWN
	ling.l_onoff = 1;
	ling.l_linger = MAX_SECONDS_WAIT_FOR_SHUTDOWN };
#else
	ling.l_onoff = 1;
	ling.l_linger = 0; 
#endif
	setsockopt( fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling) );
	
	// ���ͽ��ջ�����
#if SEND_BUFFER_SIZE > 0
//	int32_t sendbuf_size = SEND_BUFFER_SIZE;
//	setsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void *)&sendbuf_size, sizeof(sendbuf_size) );
#endif
#if RECV_BUFFER_SIZE > 0
//	int32_t recvbuf_size = RECV_BUFFER_SIZE;
//	setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void *)&recvbuf_size, sizeof(recvbuf_size) );
#endif

	return;
}

int32_t _start_direct( evsets_t sets, struct acceptor * acceptor )
{
	// ��ʼ��עaccept�¼�
	
	event_set( acceptor->event, acceptor->fd, EV_READ|EV_PERSIST );
	event_set_callback( acceptor->event, channel_on_accept, acceptor );
	evsets_add( sets, acceptor->event, 0 );

	return 0;
}

int32_t _assign_direct( struct session_manager * manager, evsets_t sets, struct stask_assign * task )
{
	int32_t rc = 0;
	int32_t key = task->fd;
	
	// �Ự����������Ự
	struct session * session = session_manager_alloc( manager, key );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, 
			"_assign_direct(fd:%d, host:'%s', port:%d) failed .", task->fd, task->host, task->port );
		close( task->fd );
		return -1;
	}
	
	// �ص��߼���, ȷ���Ƿ��������Ự
	rc = task->cb( task->context, session->id, task->host, task->port );
	if ( rc != 0 )
	{
		// �߼��㲻��������Ự
		session_manager_remove( manager, session );
		close( task->fd );
		return 1;
	}
	
	// �Ự��ʼ
	session_set_endpoint( session, task->host, task->port );
	session_start( session, eSessionType_Once, task->fd, sets );
	
	return 0;
}

int32_t _send_direct( struct session_manager * manager, struct stask_send * task )
{
	struct session * session = session_manager_get( manager, task->id );

	if ( session == NULL )
	{
		syslog(LOG_WARNING, "_send_direct(SID=%ld) failed, the Session is invalid .", task->id );
		return -1;
	}

	return session_send( session, task->buf, task->nbytes );
}

int32_t _broadcast_direct( struct session_manager * manager, struct message * msg )
{
	uint32_t i = 0;
	int32_t rc = 0;

	for ( i = 0; i < sidlist_count(msg->tolist); ++i )
	{
		sid_t id = sidlist_get(msg->tolist, i);
		struct session * session = session_manager_get( manager, id );

		if ( session == NULL )
		{
			// �Ự�Ƿ�
			message_add_failure( msg, id );
			continue;
		}

		// ֱ����ӵ��Ự�ķ����б���
		if ( session_append(session, msg) != 0 )
		{
			// ���ʧ��
			message_add_failure( msg, id );
			continue;
		}

		++rc;
	}

	// ��Ϣ����ʧ��, ֱ������
	if ( message_is_complete(msg) == 0 )
	{
		message_destroy( msg );
	}

	return rc;
}

int32_t _shutdown_direct( struct session_manager * manager, sid_t id )
{
	struct session * session = session_manager_get( manager, id );

	if ( session == NULL )
	{
		syslog(LOG_WARNING, "_shutdown_direct(SID=%ld) failed, the Session is invalid .", id );
		return -1;
	}

	return session_shutdown( session );
}

int32_t _shutdowns_direct( struct session_manager * manager, struct sidlist * ids )
{
	uint32_t i = 0;
	int32_t rc = 0;

	for ( i = 0; i < sidlist_count(ids); ++i )
	{
		sid_t id = sidlist_get(ids, i);
		struct session * session = session_manager_get( manager, id );

		if ( session == NULL )
		{
			continue;
		}

		// ֱ����ֹ
		++rc;
		session_shutdown( session );
	}

	sidlist_destroy( ids );

	return rc;
}

void _io_methods( void * context, uint8_t index, int16_t type, void * task )
{
	struct server * server = (struct server *)context;

	// ��ȡ�¼����Լ��Ự������
	evsets_t sets = iothreads_get_sets( server->group, index );
	struct session_manager * manager = _get_manager( server, index );
	
	switch ( type )
	{
	
	case eIOTaskType_Listen :
		{
			// ��һ��������
			_start_direct( sets, (struct acceptor *)task );
		}
		break;

	case eIOTaskType_Assign :
		{
			// ����һ��������
			_assign_direct( manager, sets, (struct stask_assign *)task );
		}
		break;

	case eIOTaskType_Send :
		{
			// ��������
			_send_direct( manager, (struct stask_send *)task );
		}
		break;

	case eIOTaskType_Broadcast :
		{
			// �㲥����
			_broadcast_direct( manager, (struct message *)task );
		}
		break;

	case eIOTaskType_Shutdown :
		{
			// ��ֹһ���Ự
			_shutdown_direct( manager, *( (sid_t *)task ) );
		}
		break;

	case eIOTaskType_Shutdowns :
		{
			// ������ֹ����Ự
			_shutdowns_direct( manager, (struct sidlist *)task );
		}
		break;
	
	}

	return;
}

