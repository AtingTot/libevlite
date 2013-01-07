
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "utils.h"
#include "event-internal.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// �汾
#ifndef __EVENT_VERSION__
#define __EVENT_VERSION__ "libevlite-X.X.X"
#endif

// linux��freebsd�ļ����Զ���
#if defined (__linux__)
extern const struct eventop epollops;
const struct eventop * evsel = &epollops;
#elif defined(__FreeBSD__) || defined(__APPLE__) || defined(__darwin__) || defined(__OpenBSD__)
extern const struct eventop kqueueops;
const struct eventop * evsel = &kqueueops;
#else
const struct eventop * evsel = NULL;
#endif

// event.c�еĹ��߶���

static inline int32_t evsets_process_active( struct eventset * self );
static inline int32_t event_list_insert( struct eventset * self, struct event * ev, int32_t type );
static inline int32_t event_list_remove( struct eventset * self, struct event * ev, int32_t type );

static inline void evsets_clear_now( struct eventset * self );
static inline int64_t evsets_get_now( struct eventset * self );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int32_t event_list_insert( struct eventset * self, struct event * ev, int32_t type )
{
	if ( ev->status & type )
	{
		// �¼��Ѿ����ڸ��б���
		return 1;
	}

	ev->status |= type;

	switch( type )
	{
		case EVSTATUS_INSERTED :
			{
				TAILQ_INSERT_TAIL( &(self->eventlist), ev, eventlink );
			}
			break;

		case EVSTATUS_ACTIVE :
			{
				TAILQ_INSERT_TAIL( &(self->activelist), ev, activelink );
			}
			break;

		case EVSTATUS_TIMER :
			{
				evtimer_append( self->core_timer, ev );
			}
			break;

		default :
			{
				return -1;
			}
			break;
	}

	return 0;
}

int32_t event_list_remove( struct eventset * self, struct event * ev, int32_t type )
{
	if ( !(ev->status & type) )
	{
		return -1;
	}

	ev->status &= ~type;

	switch( type )
	{
		case EVSTATUS_INSERTED :
			{
				TAILQ_REMOVE( &(self->eventlist), ev, eventlink );
			}
			break;

		case EVSTATUS_ACTIVE :
			{
				TAILQ_REMOVE( &(self->activelist), ev, activelink );
			}
			break;

		case EVSTATUS_TIMER :
			{
				evtimer_remove( self->core_timer, ev );
			}
			break;

		default :
			{
				return -2;
			}
			break;
	}

	return 0;
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


event_t event_create()
{
	struct event * self = NULL;

	self = (struct event *)malloc( sizeof(struct event) );
	if ( self )
	{
		self->fd = -1;
		self->events = 0;
		self->evsets = NULL;

		self->cb = NULL;
		self->arg = NULL;

		self->timer_index = -1;
		self->timer_msecs = -1;
		self->timer_stepcnt = -1;

		self->results = 0;
		self->status = EVSTATUS_INIT;
	}

	return (event_t)self;
}

void event_set( event_t self, int32_t fd, int16_t ev )
{
	struct event * e = (struct event *)self;

	e->fd = fd;
	e->events = ev;

	return;
}

void event_set_callback( event_t self, void (*cb)(int32_t, int16_t, void *), void * arg )
{
	struct event * e = (struct event *)self;

	e->cb = cb;
	e->arg = arg;

	return;
}

int32_t event_get_fd( event_t self )
{ 
	return ((struct event *)self)->fd;
}

evsets_t event_get_sets( event_t self )
{ 
	return ((struct event *)self)->evsets;
}

void event_destroy( event_t self )
{
	free( self );
}

int32_t event_active( struct event * self, int16_t res )
{
	if ( self->status & EVSTATUS_ACTIVE )
	{
		self->results |= res;
		return 1;
	}

	self->results = res;
	event_list_insert( self->evsets, self, EVSTATUS_ACTIVE );

	return 0;
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


evsets_t evsets_create()
{
	struct eventset * self = NULL;

	//
	// �����Լ��
	//
	assert( evsel != NULL );

	self = (struct eventset *)malloc( sizeof(struct eventset) );
	if ( self )
	{
		self->evselect = (struct eventop *)evsel;

		self->evsets = self->evselect->init();
		if ( self->evsets )
		{
			self->core_timer = evtimer_create( TIMER_MAX_PRECISION, TIMER_BUCKET_COUNT );
			if ( self->core_timer )
			{
				TAILQ_INIT( &self->eventlist );
				TAILQ_INIT( &self->activelist );

				self->timer_precision = TIMER_MAX_PRECISION;
				self->expire_time = mtime() + self->timer_precision;
			}
			else
			{
				evsets_destroy( self );
				self = NULL;
			}
		}
		else
		{
			evsets_destroy( self );
			self = NULL;
		}
	}

	return self;
}

const char * evsets_get_version()
{
	return __EVENT_VERSION__;
}

int32_t evsets_add( evsets_t self, event_t ev, int32_t tv )
{
	int32_t rc = 1;
	struct event * e = (struct event *)ev;
	struct eventset * sets = (struct eventset *)self;

	assert( e->cb != NULL );

	if ( e->status & ~EVSTATUS_ALL )
	{
		return -1;
	}

	e->evsets = self;

	if ( (e->events & (EV_READ|EV_WRITE))
			&& !(e->status & (EVSTATUS_ACTIVE|EVSTATUS_INSERTED)) )
	{
		rc = sets->evselect->add( sets->evsets, e );
		if ( rc == 0 )
		{
			event_list_insert( sets, e, EVSTATUS_INSERTED );
		}
	}

	if ( rc >= 0 && tv > 0 )
	{
		// ����Ѿ��ڶ�ʱ������,
		// һ��Ҫɾ�����Ե�ǰʱ�䵱ǰ�������¼��붨ʱ����
		// �����Ƚ�׼ȷ
		if ( e->status & EVSTATUS_TIMER )
		{
			event_list_remove( sets, e, EVSTATUS_TIMER );
		}

		// �Ѿ���ʱ��
		// ����Ӽ��������ɾ����
		// ���¼��붨ʱ����
		if ( (e->results & EV_TIMEOUT)
				&& (e->status & EVSTATUS_ACTIVE) )
		{
			event_list_remove( sets, e, EVSTATUS_ACTIVE );
		}

		rc = 0;
		e->timer_msecs = tv < sets->timer_precision ? sets->timer_precision : tv;
		event_list_insert( sets, e, EVSTATUS_TIMER );
	}

	return rc;
}

int32_t evsets_del( evsets_t self, event_t ev )
{
	struct event * e = (struct event *)ev;
	struct eventset * sets = (struct eventset *)self;

	assert ( e->evsets == sets );

	if ( e->status & ~EVSTATUS_ALL )
	{
		return -2;
	}

	if ( e->status & EVSTATUS_TIMER )
	{
		e->timer_msecs = -1;
		event_list_remove( sets, e, EVSTATUS_TIMER );
	}
	if ( e->status & EVSTATUS_ACTIVE )
	{
		event_list_remove( sets, e, EVSTATUS_ACTIVE );
	}
	if ( e->status & EVSTATUS_INSERTED )
	{
		event_list_remove( sets, e, EVSTATUS_INSERTED );
		return sets->evselect->del( sets->evsets, e );
	}

	return 0;
}

int32_t evsets_dispatch( evsets_t self )
{
	int32_t res = 0;
	int32_t seconds4wait = 0;
	struct eventset * sets = (struct eventset *)self;

	// ���ʱ�仺��
	evsets_clear_now( sets );

	// û�м����¼�������µȴ���ʱʱ��	
	if ( TAILQ_EMPTY(&sets->activelist) )
	{
		// ���ݶ�ʱ���ĳ�ʱʱ��, ȷ��IO�ĵȴ�ʱ��
		seconds4wait = (int32_t)( sets->expire_time - evsets_get_now(sets) );
		if ( seconds4wait < 0 )
		{
			seconds4wait = 0;
		}
		else if ( seconds4wait > sets->timer_precision )
		{
			seconds4wait = sets->timer_precision;
		}
	}

	// ����IO�¼�
	res = sets->evselect->dispatch( sets, sets->evsets, seconds4wait );
	if ( res < 0 )
	{
		return -1;
	}

	// �¼����ĳ�ʱʱ����Ҫ��ʱ���µ�
	evsets_clear_now( sets );
	if ( sets->expire_time <= evsets_get_now(sets) )
	{
		// ��ʱ��ʱ�䵽��, �ַ��¼�
		res += evtimer_dispatch( sets->core_timer );
		sets->expire_time = evsets_get_now(sets) + sets->timer_precision;
	}

	// ���������¼�, ���ص�����õĺ���
	return evsets_process_active( sets );
}

void evsets_clear_now( struct eventset * self )
{
	self->cache_now = 0;
}

int64_t evsets_get_now( struct eventset * self )
{
	if ( self->cache_now == 0 )
	{
		self->cache_now = mtime();
	}

	return self->cache_now;
}

void evsets_destroy( evsets_t self )
{
	struct event * ev = NULL;
	struct eventset * sets = (struct eventset *)self;

	// ɾ�������¼�
	for ( ev = TAILQ_FIRST( &(sets->eventlist) ); ev; )
	{
		struct event * next = TAILQ_NEXT( ev, eventlink );

		if ( !(ev->status & EVSTATUS_INTERNAL) )
		{
			evsets_del( self, (event_t)ev );
		}

		ev = next;
	}

	// �����ʱ��
	if ( sets->core_timer )
	{
		evtimer_clean( sets->core_timer );
	}

	// ɾ�������¼�
	for ( ev = TAILQ_FIRST( &(sets->activelist) ); ev; )
	{
		struct event * next = TAILQ_NEXT( ev, activelink );

		if ( !(ev->status & EVSTATUS_INTERNAL) )
		{
			evsets_del( self, (event_t)ev );
		}

		ev = next;
	}

	// ���ٶ�ʱ��
	if ( sets->core_timer )
	{
		evtimer_destroy( sets->core_timer );
	}

	// ����IOʵ��
	sets->evselect->final( sets->evsets );
	free( sets );

	return ;
}

int32_t evsets_process_active( struct eventset * self )
{
	int32_t rc = 0;
	struct event * ev = NULL;
	struct event_list * activelist = &(self->activelist);

	for ( ev = TAILQ_FIRST(activelist); ev; ev = TAILQ_FIRST(activelist) )
	{
		if ( !(ev->events&EV_PERSIST) )
		{
			evsets_del( self, (event_t)ev );
		}
		else
		{
			event_list_remove( self, ev, EVSTATUS_ACTIVE );

			// Timeouts and persistent events work together
			if ( ev->results&EV_TIMEOUT )
			{
				event_list_insert( self, ev, EVSTATUS_TIMER );
			}
		}

		// �ص�
		++rc;
		(*ev->cb)( event_get_fd((event_t)ev), ev->results, ev->arg );
	}

	return rc;
}

