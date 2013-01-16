
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "utils.h"
#include "threads.h"
#include "threads-internal.h"

// ���������߳���
// nthreads		- �����߳����е��߳���
// method		- ��������
iothreads_t iothreads_start( uint8_t nthreads, 
		void (*method)(void *, uint8_t, int16_t, void *), void * context )
{
	uint8_t i = 0;
	struct iothreads * iothreads = NULL;

	iothreads = calloc( 1, sizeof(struct iothreads) );
	if ( iothreads == NULL )
	{
		return NULL;
	}

	iothreads->threadgroup = calloc( nthreads, sizeof(struct iothread) );
	if ( iothreads->threadgroup == NULL )
	{
		free( iothreads );
		iothreads = NULL;
	}

	iothreads->method 	= method;
	iothreads->context	= context;
	iothreads->nthreads = nthreads;
	pthread_cond_init( &iothreads->cond, NULL );
	pthread_mutex_init( &iothreads->lock, NULL );

	// ���������߳�
	iothreads->runflags = 1;
	iothreads->nrunthreads = nthreads;
	for ( i = 0; i < nthreads; ++i )
	{
		iothread_start( iothreads->threadgroup+i, i, iothreads );
	}

	return iothreads;
}

pthread_t iothreads_get_id( iothreads_t self, uint8_t index )
{
	struct iothreads * iothreads = (struct iothreads *)(self);
	assert( index < iothreads->nthreads );
	return iothreads->threadgroup[index].id;
}

evsets_t iothreads_get_sets( iothreads_t self, uint8_t index )
{
	struct iothreads * iothreads = (struct iothreads *)(self);
	assert( index < iothreads->nthreads );
	return iothreads->threadgroup[index].sets;
}

// �������߳�����ָ�����߳��ύ����
// index	- ָ�������̵߳ı��
// type		- �ύ����������
// task		- �ύ����������
// size		- �������ݵĳ���, Ĭ������Ϊ0
int32_t iothreads_post( iothreads_t self, uint8_t index, int16_t type, void * task, uint8_t size )
{
	struct iothreads * iothreads = (struct iothreads *)(self);

	assert( size <= TASK_PADDING_SIZE );
	assert( index < iothreads->nthreads );

	if ( iothreads->runflags != 1 )
	{
		return -1;
	}

	return iothread_post( iothreads->threadgroup+index, (size == 0 ? eTaskType_User : eTaskType_Data), type, task, size );
}

void iothreads_stop( iothreads_t self )
{
	uint8_t i = 0;
	struct iothreads * iothreads = (struct iothreads *)(self);

	// �������̷߳���ֹͣ����
	iothreads->runflags = 0;
	for ( i = 0; i < iothreads->nthreads; ++i )
	{
		iothread_post( iothreads->threadgroup+i, eTaskType_Null, 0, NULL, 0 );
	}

	// �ȴ��߳��˳�
	pthread_mutex_lock( &iothreads->lock );
	while ( iothreads->nrunthreads > 0 )
	{
		pthread_cond_wait( &iothreads->cond, &iothreads->lock );
	}
	pthread_mutex_unlock( &iothreads->lock );

	// �������������߳�
	for ( i = 0; i < iothreads->nthreads; ++i )
	{
		iothread_stop( iothreads->threadgroup + i );
	}

	pthread_cond_destroy( &iothreads->cond );
	pthread_mutex_destroy( &iothreads->lock );
	if ( iothreads->threadgroup )
	{
		free( iothreads->threadgroup );
		iothreads->threadgroup = NULL;
	}

	free ( iothreads );

	return;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static void * iothread_main( void * arg );
static void iothread_on_command( int32_t fd, int16_t ev, void * arg );

int32_t iothread_start( struct iothread * self, uint8_t index, iothreads_t parent )
{
	self->index = index;
	self->parent = parent;

	self->sets = evsets_create();
	if ( self->sets == NULL )
	{
		iothread_stop(self);
		return -1;
	}

	self->cmdevent = event_create();
	self->queue = msgqueue_create( MSGQUEUE_DEFAULT_SIZE );
	if ( self->queue == NULL || self->cmdevent == NULL )
	{
		iothread_stop(self);
		return -2;
	}

	// ��ʼ�������¼�
	event_set( self->cmdevent, msgqueue_popfd(self->queue), EV_READ|EV_PERSIST );
	event_set_callback( self->cmdevent, iothread_on_command, self );
	evsets_add( self->sets, self->cmdevent, 0 );

	// �����߳�
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	//	assert( pthread_attr_setstacksize( &attr, THREAD_DEFAULT_STACK_SIZE ) );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

	int32_t rc = pthread_create(&(self->id), &attr, iothread_main, self);
	pthread_attr_destroy( &attr );

	if ( rc != 0 )
	{
		iothread_stop(self);
		return -3;
	}

	return 0;
}

int32_t iothread_post( struct iothread * self, int16_t type, int16_t utype, void * task, uint8_t size )
{
	struct task inter_task;

	inter_task.type		= type;
	inter_task.utype	= utype;

	if ( size == 0 )
	{
		inter_task.taskdata = task;
	}
	else
	{
		memcpy( &(inter_task.data), task, size );
	}

	// Ĭ��: �ύ��������������
	return msgqueue_push( self->queue, &inter_task, POST_IOTASK_AND_NOTIFY );
}

int32_t iothread_stop( struct iothread * self )
{
	if ( self->queue )
	{
		msgqueue_destroy( self->queue );
		self->queue = NULL;
	}

	if ( self->cmdevent )
	{
		evsets_del( self->sets, self->cmdevent );
		event_destroy( self->cmdevent );
		self->cmdevent = NULL;
	}

	if ( self->sets )
	{
		evsets_destroy( self->sets );
		self->sets = NULL;
	}

	return 0;
}

void * iothread_main( void * arg )
{
	struct iothread * thread = (struct iothread *)arg;
	struct iothreads * parent = (struct iothreads *)(thread->parent);

	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	// ��ʼ������	
	struct taskqueue doqueue;
	QUEUE_INIT(taskqueue)( &doqueue, MSGQUEUE_DEFAULT_SIZE );

	while ( parent->runflags )
	{
		// ��ѯ�����¼�
		evsets_dispatch( thread->sets );

		// �����������
		msgqueue_swap( thread->queue, &doqueue );

		// ��������
		while ( QUEUE_COUNT(taskqueue)(&doqueue) > 0 )
		{
			struct task task;
			void * data = NULL;

			QUEUE_POP(taskqueue)( &doqueue, &task );
			switch ( task.type )
			{
				case eTaskType_Null :
					{
						// ������
						continue;
					}
					break;

				case eTaskType_User :
					{
						// �û�����
						data = task.taskdata;
					}
					break;

				case eTaskType_Data :
					{
						// ��������
						data = (void *)(task.data);
					}
					break;
			}
			// �ص�
			parent->method( parent->context, thread->index, task.utype, data );
		}
	}

	QUEUE_CLEAR(taskqueue)( &doqueue );

	// �����̷߳�����ֹ�ź�
	pthread_mutex_lock( &parent->lock );
	--parent->nrunthreads;
	pthread_cond_signal( &parent->cond );
	pthread_mutex_unlock( &parent->lock );

	return NULL;
}

void iothread_on_command( int32_t fd, int16_t ev, void * arg )
{
	//struct iothread * iothread = (struct iothread *)arg;

	if ( ev & EV_READ )
	{	
		char buf[ 64 ];		
		int32_t nread = 0;

		nread = read( fd, buf, sizeof(buf) );
		if ( nread == -1 )
		{
			//
		}
	}

	return;
}

