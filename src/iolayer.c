
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "network.h"
#include "channel.h"
#include "session.h"
#include "network-internal.h"

#include "iolayer.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static int32_t _listen_direct( evsets_t sets, struct acceptor * acceptor );
static int32_t _connect_direct( evsets_t sets, struct connector * connector );
static void _reconnect_direct( int32_t fd, int16_t ev, void * arg );
static int32_t _assign_direct( struct session_manager * manager, evsets_t sets, struct task_assign * task );
static int32_t _send_direct( struct session_manager * manager, struct task_send * task );
static int32_t _broadcast_direct( struct session_manager * manager, struct message * msg );
static int32_t _shutdown_direct( struct session_manager * manager, sid_t id );
static int32_t _shutdowns_direct( struct session_manager * manager, struct sidlist * ids );

static void _socket_option( int32_t fd );
static void _io_methods( void * context, uint8_t index, int16_t type, void * task );

static inline int32_t _new_managers( struct iolayer * self );
static inline struct session_manager * _get_manager( struct iolayer * self, uint8_t index );
static inline void _dispatch_sidlist( struct iolayer * self, struct sidlist ** listgroup, sid_t * ids, uint32_t count );
static inline int32_t _send_buffer( struct iolayer * self, sid_t id, const char * buf, uint32_t nbytes, int32_t isfree );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// ��������ͨ�Ų�
iolayer_t iolayer_create( uint8_t nthreads, uint32_t nclients )
{
	struct iolayer * self = (struct iolayer *)calloc( 1, sizeof(struct iolayer) );
	if ( self == NULL )
	{
		return NULL;
	}

	self->nthreads = nthreads;
	self->nclients = nclients;

	// ��ʼ���Ự������
	if ( _new_managers( self ) != 0 )
	{
		iolayer_destroy( self );
		return NULL;
	}

	// ���������߳���
	self->group = iothreads_start( self->nthreads, _io_methods, self );
	if ( self->group == NULL )
	{
		iolayer_destroy( self );
		return NULL;
	}

	return self;
}

// ��������ͨ�Ų�
void iolayer_destroy( iolayer_t self )
{
	struct iolayer * layer = (struct iolayer *)self;

	// ֹͣ�����߳���
	if ( layer->group )
	{
		iothreads_stop( layer->group );
	}

	// ���ٹ�����
	if ( layer->managers )
	{
		uint8_t i = 0;
		for ( i = 0; i < layer->nthreads; ++i )
		{
			struct session_manager * manager = layer->managers[i<<3];	
			if ( manager )
			{
				session_manager_destroy( manager );
			}
		}
		free( layer->managers );
		layer->managers = NULL;
	}

	free( layer );

	return; 
}

// ����������
//		host		- �󶨵ĵ�ַ
//		port		- �����Ķ˿ں�
//		cb			- �»Ự�����ɹ���Ļص�,�ᱻ��������̵߳��� 
//							����1: �����Ĳ���; 
//							����2: �»ỰID; 
//							����3: �Ự��IP��ַ; 
//							����4: �Ự�Ķ˿ں�
//		context		- �����Ĳ���
int32_t iolayer_listen( iolayer_t self, 
		const char * host, uint16_t port, 
		int32_t (*cb)( void *, sid_t, const char * , uint16_t ), void * context )
{
	struct iolayer * layer = (struct iolayer *)self;

	struct acceptor * acceptor = calloc( 1, sizeof(struct acceptor) );
	if ( acceptor == NULL )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, Out-Of-Memory .", __FUNCTION__, host, port);
		return -1;
	}

	acceptor->event = event_create();
	if ( acceptor->event == NULL )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, can't create AcceptEvent.", __FUNCTION__, host, port);
		return -2;
	}

	acceptor->fd = tcp_listen( (char *)host, port, _socket_option );
	if ( acceptor->fd <= 0 )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, tcp_listen() failure .", __FUNCTION__, host, port);
		return -3;
	}

	acceptor->cb = cb;
	acceptor->context = context;
	acceptor->parent = self;
	acceptor->port = port;
	acceptor->host[0] = 0;
	if ( host != NULL )
	{
		strncpy( acceptor->host, host, INET_ADDRSTRLEN );
	}

	iothreads_post( layer->group, (acceptor->fd%layer->nthreads), eIOTaskType_Listen, acceptor, 0 );	

	return 0;
}

// �ͻ��˿���
//		host		- Զ�̷������ĵ�ַ
//		port		- Զ�̷������Ķ˿�
//		seconds		- ���ӳ�ʱʱ��
//		cb			- ���ӽ���Ļص�
//							����1: �����Ĳ���
//							����2: ���ӽ��
//							����3: ���ӵ�Զ�̷������ĵ�ַ
//							����4: ���ӵ�Զ�̷������Ķ˿�
//							����5: ���ӳɹ��󷵻صĻỰID
//		context		- �����Ĳ���
int32_t iolayer_connect( iolayer_t self,
		const char * host, uint16_t port, int32_t seconds, 
		int32_t (*cb)( void *, int32_t, const char *, uint16_t , sid_t), void * context	)
{
	struct iolayer * layer = (struct iolayer *)self;

	struct connector * connector = calloc( 1, sizeof(struct connector) ); 
	if ( connector == NULL )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, Out-Of-Memory .", __FUNCTION__, host, port);
		return -1;
	}

	connector->event = event_create();
	if ( connector->event == NULL )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, can't create ConnectEvent.", __FUNCTION__, host, port);
		return -2;
	}

	connector->fd = tcp_connect( (char *)host, port, 1 );
	if ( connector->fd <= 0 )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, tcp_connect() failure .", __FUNCTION__, host, port);
		return -3;
	}

	connector->cb = cb;
	connector->context = context;
	connector->mseconds = seconds*1000;
	connector->parent = self;
	connector->port = port;
	strncpy( connector->host, host, INET_ADDRSTRLEN );

	iothreads_post( layer->group, (connector->fd%layer->nthreads), eIOTaskType_Connect, connector, 0 );	

	return 0;
}

int32_t iolayer_set_timeout( iolayer_t self, sid_t id, int32_t seconds )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct iolayer * layer = (struct iolayer *)self;

	if ( index >= layer->nthreads )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's index[%u] is invalid .", __FUNCTION__, id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( layer, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's manager[%u] is invalid .", __FUNCTION__, id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session is invalid .", __FUNCTION__, id );
		return -3;
	}

	session->setting.timeout_msecs = seconds*1000;

	return 0;
}

int32_t iolayer_set_keepalive( iolayer_t self, sid_t id, int32_t seconds )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct iolayer * layer = (struct iolayer *)self;

	if ( index >= layer->nthreads )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's index[%u] is invalid .", __FUNCTION__, id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( layer, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's manager[%u] is invalid .", __FUNCTION__, id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session is invalid .", __FUNCTION__, id );
		return -3;
	}

	session->setting.keepalive_msecs = seconds*1000;

	return 0;
}

int32_t iolayer_set_service( iolayer_t self, sid_t id, ioservice_t * service, void * context )
{
	// NOT Thread-Safe
	uint8_t index = SID_INDEX(id);
	struct iolayer * layer = (struct iolayer *)self;

	if ( index >= layer->nthreads )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's index[%u] is invalid .", __FUNCTION__, id, index );
		return -1;
	}

	struct session_manager * manager = _get_manager( layer, index );
	if ( manager == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's manager[%u] is invalid .", __FUNCTION__, id, index );
		return -2;
	}

	struct session * session = session_manager_get( manager, id );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session is invalid .", __FUNCTION__, id );
		return -3;
	}

	session->context = context;
	session->service = *service;

	return 0;
}

int32_t iolayer_send( iolayer_t self, sid_t id, const char * buf, uint32_t nbytes, int32_t isfree )
{
	return _send_buffer( (struct iolayer *)self, id, buf, nbytes, isfree );
}

int32_t iolayer_broadcast( iolayer_t self, sid_t * ids, uint32_t count, const char * buf, uint32_t nbytes )
{
	// ��Ҫ����ids
	uint8_t i = 0;
	int32_t rc = 0;

	pthread_t threadid = pthread_self();
	struct sidlist * listgroup[ 256 ] = {NULL};
	struct iolayer * layer = (struct iolayer *)self;

	_dispatch_sidlist( layer, listgroup, ids, count );

	for ( i = 0; i < layer->nthreads; ++i )
	{
		if ( listgroup[i] == NULL )
		{
			continue;
		}

		uint32_t count = sidlist_count( listgroup[i] );

		// p2p

		if ( count == 1 )
		{
			sid_t id = sidlist_get( listgroup[i], 0 );
			if ( _send_buffer( layer, id, buf, nbytes, 0 ) == 0 )
			{
				rc += 1;
			}
			// ����listgroup[i]
			sidlist_destroy( listgroup[i] );
			continue;
		}

		// broadcast

		struct message * msg = message_create();
		if ( msg == NULL )
		{
			sidlist_destroy( listgroup[i] );
			continue;
		}
		message_add_buffer( msg, (char *)buf, nbytes );
		message_set_receivers( msg, listgroup[i] );

		if ( threadid == iothreads_get_id( layer->group, i ) )
		{
			// ���߳���ֱ�ӹ㲥
			_broadcast_direct( _get_manager(layer, i), msg );
		}
		else
		{
			// ���߳��ύ�㲥����
			int32_t result = iothreads_post( layer->group, i, eIOTaskType_Broadcast, msg, 0 );
			if ( result != 0 )
			{
				message_destroy( msg );
				continue;
			}
		}

		// listgroup[i] ����message������
		rc += count;
	}

	return rc;
}

int32_t iolayer_shutdown( iolayer_t self, sid_t id )
{
	uint8_t index = SID_INDEX(id);
	struct iolayer * layer = (struct iolayer *)self;

	if ( index >= layer->nthreads )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's index[%u] is invalid .", __FUNCTION__, id, index );
		return -1;
	}

	// �����ڻص�������ֱ����ֹ�Ự
	// ���������������ԻỰ�Ĳ������Ƿ���
#if 0
	if ( pthread_self() == iothreads_get_id( layer->group, index ) )
	{
		// ���߳���ֱ����ֹ
		return _shutdown_direct( _get_manager(layer, index), &task );
	}
#endif

	// ���߳��ύ��ֹ����
	return iothreads_post( layer->group, index, eIOTaskType_Shutdown, (void *)&id, sizeof(id) );

}

int32_t iolayer_shutdowns( iolayer_t self, sid_t * ids, uint32_t count )
{
	// ��Ҫ����ids
	uint8_t i = 0;
	int32_t rc = 0;

	struct sidlist * listgroup[ 256 ] = {NULL};
	struct iolayer * layer = (struct iolayer *)self;

	_dispatch_sidlist( layer, listgroup, ids, count );

	for ( i = 0; i < layer->nthreads; ++i )
	{
		if ( listgroup[i] == NULL )
		{
			continue;
		}

		// ����iolayer_shutdown()

		// ���߳��ύ������ֹ����
		int32_t result = iothreads_post( layer->group, i, eIOTaskType_Shutdowns, listgroup[i], 0 );
		if ( result != 0 )
		{
			sidlist_destroy( listgroup[i] );
			continue;
		}

		rc += sidlist_count( listgroup[i] );
	}

	return rc;
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct session * iolayer_alloc_session( struct iolayer * self, int32_t key )
{
	uint8_t index = key % self->nthreads;

	struct session * session = NULL;
	struct session_manager * manager = _get_manager( self, index );

	if ( manager )
	{
		session = session_manager_alloc( manager );
	}
	else
	{
		syslog(LOG_WARNING, "%s(fd=%d) failed, the SessionManager's index[%d] is invalid .", __FUNCTION__, key, index );
	}

	return session;
}

int32_t iolayer_reconnect( struct iolayer * self, struct connector * connector )
{
	// ���ȱ����ȹر���ǰ��������
	if ( connector->fd > 0 )
	{
		close( connector->fd );
		connector->fd = -1;
	}

	// 2���������, ����æ��
	event_set( connector->event, -1, 0 );
	event_set_callback( connector->event, _reconnect_direct, connector );
	evsets_add( connector->evsets, connector->event, TRY_RECONNECT_INTERVAL );

	return 0;
}

int32_t iolayer_free_connector( struct iolayer * self, struct connector * connector )
{
	if ( connector->event )
	{
		evsets_del( connector->evsets, connector->event );
		event_destroy( connector->event );
		connector->event = NULL;
	}

	if ( connector->fd > 0 )
	{
		close( connector->fd );
		connector->fd = -1;
	}

	free( connector );
	return 0;
}

int32_t iolayer_assign_session( struct iolayer * self, uint8_t index, struct task_assign * task )
{
	evsets_t sets = iothreads_get_sets( self->group, index );
	pthread_t threadid = iothreads_get_id( self->group, index );

	if ( pthread_self() == threadid )
	{
		return _assign_direct( _get_manager(self, index), sets, task );
	}

	// ���߳��ύ��������
	return iothreads_post( self->group, index, eIOTaskType_Assign, task, sizeof(struct task_assign) );
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int32_t _new_managers( struct iolayer * self )
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

struct session_manager * _get_manager( struct iolayer * self, uint8_t index )
{
	if ( index >= self->nthreads )
	{
		return NULL;
	}

	return (struct session_manager *)( self->managers[index<<3] );
}

void _dispatch_sidlist( struct iolayer * self, struct sidlist ** listgroup, sid_t * ids, uint32_t count )
{
	uint32_t i = 0;

	for ( i = 0; i < count; ++i )
	{
		uint8_t index = SID_INDEX( ids[i] );

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

int32_t _send_buffer( struct iolayer * self, sid_t id, const char * buf, uint32_t nbytes, int32_t isfree )
{
	int32_t result = 0;
	uint8_t index = SID_INDEX(id);

	if ( index >= self->nthreads )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session's index[%u] is invalid .", __FUNCTION__, id, index );
		return -1;
	}

	struct task_send task;
	task.id		= id;
	task.nbytes	= nbytes;
	task.isfree	= isfree;
	task.buf	= (char *)buf;

	if ( pthread_self() == iothreads_get_id( self->group, index ) )
	{
		return _send_direct( _get_manager(self, index), &task );
	}

	// ���߳��ύ��������

	if ( isfree == 0 )
	{
		task.buf = (char *)malloc( nbytes );
		if ( task.buf == NULL )
		{
			syslog(LOG_WARNING, "%s(SID=%ld) failed, can't allocate the memory for '_buf' .", __FUNCTION__, id );
			return -2;
		}

		task.isfree = 1;
		memcpy( task.buf, buf, nbytes );
	}

	result = iothreads_post( self->group, index, eIOTaskType_Send, (void *)&task, sizeof(task) );
	if ( result != 0 )
	{
		free( task.buf );
	}

	return result;
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
	ling.l_linger = MAX_SECONDS_WAIT_FOR_SHUTDOWN;
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

int32_t _listen_direct( evsets_t sets, struct acceptor * acceptor )
{
	// ��ʼ��עaccept�¼�

	event_set( acceptor->event, acceptor->fd, EV_READ|EV_PERSIST );
	event_set_callback( acceptor->event, channel_on_accept, acceptor );
	evsets_add( sets, acceptor->event, 0 );

	return 0;
}

int32_t _connect_direct( evsets_t sets, struct connector * connector )
{
	// ��ʼ��ע�����¼�
	connector->evsets = sets;

	event_set( connector->event, connector->fd, EV_WRITE );
	event_set_callback( connector->event, channel_on_connected, connector );
	evsets_add( sets, connector->event, connector->mseconds );

	return 0;
}

void _reconnect_direct( int32_t fd, int16_t ev, void * arg )
{
	struct connector * connector = (struct connector *)arg;

	// ������������
	connector->fd = tcp_connect( connector->host, connector->port, 1 );
	if ( connector->fd < 0 )
	{
		syslog(LOG_WARNING, "%s(host:'%s', port:%d) failed, tcp_connect() failure .", __FUNCTION__, connector->host, connector->port);
	}

	_connect_direct( connector->evsets, connector );
	return;
}

int32_t _assign_direct( struct session_manager * manager, evsets_t sets, struct task_assign * task )
{
	int32_t rc = 0;

	// �Ự����������Ự
	struct session * session = session_manager_alloc( manager );
	if ( session == NULL )
	{
		syslog(LOG_WARNING, 
				"%s(fd:%d, host:'%s', port:%d) failed .", __FUNCTION__, task->fd, task->host, task->port );
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

	session_set_endpoint( session, task->host, task->port );
	session_start( session, eSessionType_Once, task->fd, sets );

	return 0;
}

int32_t _send_direct( struct session_manager * manager, struct task_send * task )
{
	int32_t rc = -1;
	struct session * session = session_manager_get( manager, task->id );

	if ( session )
	{
		rc = session_send( session, task->buf, task->nbytes );
	}
	else
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session is invalid .", __FUNCTION__, task->id );
	}

	if ( task->isfree != 0 ) 
	{
		// ָ���ײ��ͷ�
		free( task->buf );
	}

	return rc;
}

int32_t _broadcast_direct( struct session_manager * manager, struct message * msg )
{
	uint32_t i = 0;
	int32_t count = 0;

	for ( i = 0; i < sidlist_count(msg->tolist); ++i )
	{
		sid_t id = sidlist_get(msg->tolist, i);
		struct session * session = session_manager_get( manager, id );

		if ( session == NULL )
		{
			message_add_failure( msg, id );
			continue;
		}

		int32_t rc = session_append( session, msg );
		if ( rc >= 0 )
		{
			// ���Ե�������
			// ��ӵ����Ͷ��гɹ�
			++count;
		}
	}

	// ��Ϣ�������, ֱ������
	if ( message_is_complete(msg) )
	{
		message_destroy( msg );
	}

	return count;
}

int32_t _shutdown_direct( struct session_manager * manager, sid_t id )
{
	struct session * session = session_manager_get( manager, id );

	if ( session == NULL )
	{
		syslog(LOG_WARNING, "%s(SID=%ld) failed, the Session is invalid .", __FUNCTION__,id );
		return -1;
	}

	return session_shutdown( session );
}

int32_t _shutdowns_direct( struct session_manager * manager, struct sidlist * ids )
{
	uint32_t i = 0;
	int32_t count = 0;

	for ( i = 0; i < sidlist_count(ids); ++i )
	{
		sid_t id = sidlist_get(ids, i);
		struct session * session = session_manager_get( manager, id );

		if ( session == NULL )
		{
			continue;
		}

		// ֱ����ֹ
		++count;
		session_shutdown( session );
	}

	sidlist_destroy( ids );

	return count;
}

void _io_methods( void * context, uint8_t index, int16_t type, void * task )
{
	struct iolayer * layer = (struct iolayer *)context;

	// ��ȡ�¼����Լ��Ự������
	evsets_t sets = iothreads_get_sets( layer->group, index );
	struct session_manager * manager = _get_manager( layer, index );

	switch ( type )
	{
		case eIOTaskType_Listen :
			{
				// ��һ��������
				_listen_direct( sets, (struct acceptor *)task );
			}
			break;

		case eIOTaskType_Connect :
			{
				// ����Զ�̷�����
				_connect_direct( sets, (struct connector *)task );
			}
			break;

		case eIOTaskType_Assign :
			{
				// ����һ��������
				_assign_direct( manager, sets, (struct task_assign *)task );
			}
			break;

		case eIOTaskType_Send :
			{
				// ��������
				_send_direct( manager, (struct task_send *)task );
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

