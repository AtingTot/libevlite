
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "event-internal.h"

//
// epoll��ע����������, ���Ƕ�д�¼�
// Ϊ��ģ���д�¼�, ����������һ���¼���
//
struct eventpair
{
	struct event * evread;
	struct event * evwrite;
};

struct epoller
{
	// ����������¼���, ����ǻ�����������
	int32_t npairs;
	struct eventpair * evpairs;

	// ����̶���Ŀ�ļ����¼�
	int32_t nevents;
	struct epoll_event * events;

	// epoll������
	int32_t epollfd;
};

void * epoll_init();
int32_t epoll_add( void * arg, struct event * ev );
int32_t epoll_del( void * arg, struct event * ev );
int32_t epoll_dispatch( struct eventset * sets, void * arg, int32_t tv );
void epoll_final( void * arg );

int32_t epoll_expand( struct epoller * self );
int32_t epoll_insert( struct epoller * self, int32_t max );

const struct eventop epollops = {
	epoll_init,
	epoll_add,
	epoll_del,
	epoll_dispatch,
	epoll_final
};

#define MAX_EPOLL_WAIT  35*60*1000

void * epoll_init()
{
	int32_t epollfd = -1;
	struct epoller * poller = NULL;

	epollfd = epoll_create(64000);      // �ò��������ں����Ѿ�ȡ��
	if ( epollfd == -1 )
	{
		return NULL;
	}

	fcntl( epollfd, F_SETFD, 1 );

	poller = (struct epoller *)malloc( sizeof(struct epoller) );
	if ( poller == NULL )
	{
		close( epollfd );
		return NULL;
	}

	poller->epollfd = epollfd;

	poller->evpairs = (struct eventpair *)calloc( INIT_EVENTS, sizeof(struct eventpair) );
	poller->events = (struct epoll_event *)calloc( INIT_EVENTS, sizeof(struct epoll_event) );
	if ( poller->evpairs == NULL || poller->events == NULL )
	{
		epoll_final( poller );
		return NULL;
	}

	poller->npairs = INIT_EVENTS;
	poller->nevents = INIT_EVENTS;

	return poller;
}

int32_t epoll_expand( struct epoller * self )
{
	int32_t nevents = self->nevents;
	struct epoll_event * newevents = NULL;

	nevents <<= 1;

	newevents = realloc( self->events, nevents*sizeof(struct epoll_event) );
	if ( newevents == NULL )
	{
		return -1;
	}

	self->nevents = nevents;
	self->events = newevents;

	return 0;
}

int32_t epoll_insert( struct epoller * self, int32_t max )
{
	int32_t npairs = 0;
	struct eventpair * pairs = NULL;

	npairs = self->npairs;
	while ( npairs <= max )
	{
		npairs <<= 1;
	}

	pairs = realloc( self->evpairs, npairs*sizeof(struct eventpair) );
	if ( pairs == NULL )
	{
		return -1;
	}

	self->evpairs = pairs;
	memset( pairs+self->npairs, 0, (npairs-self->npairs)*sizeof(struct eventpair) );
	self->npairs = npairs;

	return 0;
}

int32_t epoll_add( void * arg, struct event * ev )
{
	int32_t fd = 0;
	int32_t op = 0, events = 0;

	struct epoll_event epollevent;
	struct eventpair * eventpair = NULL;
	struct epoller * poller = (struct epoller *)arg;

	fd = event_get_fd( (event_t)ev );
	if ( fd >= poller->npairs )
	{
		if ( epoll_insert( poller, fd ) != 0 )
		{
			return -1;
		}
	}

	events = 0;
	op = EPOLL_CTL_ADD;
	eventpair = &( poller->evpairs[fd] );

	if ( eventpair->evread != NULL )
	{
		events |= EPOLLIN;
		op = EPOLL_CTL_MOD;
	}
	if ( eventpair->evwrite != NULL )
	{
		events |= EPOLLOUT;
		op = EPOLL_CTL_MOD;
	}

	if ( ev->events & EV_READ )
	{
		events |= EPOLLIN;
	}
	if ( ev->events & EV_WRITE )
	{
		events |= EPOLLOUT;
	}

	epollevent.data.fd = fd;
	epollevent.events = events;

	if ( epoll_ctl( poller->epollfd, op, fd, &epollevent ) == -1 )
	{
		return -2;
	}

	if ( ev->events & EV_READ )
	{
		eventpair->evread = ev;
	}
	if ( ev->events & EV_WRITE )
	{
		eventpair->evwrite = ev;
	}

	return 0;
}

int32_t epoll_del( void * arg, struct event * ev )
{
	int32_t fd = 0;
	int32_t op = 0, events = 0;
	int32_t delwrite = 1, delread = 1;

	struct epoll_event epollevent;
	struct eventpair * eventpair = NULL;
	struct epoller * poller = (struct epoller *)arg;

	fd = event_get_fd((event_t)ev);
	if ( fd >= poller->npairs )
	{
		return -1;
	}

	// �򵥵�ɾ��ָ�����¼�
	events = 0;
	op = EPOLL_CTL_DEL;
	eventpair = &( poller->evpairs[fd] );

	if ( ev->events & EV_READ )
	{
		events |= EPOLLIN;
	}
	if ( ev->events & EV_WRITE )
	{
		events |= EPOLLOUT;
	}

	// ɾ����ͬʱ�鿴���������Ƿ�֧���������¼�
	// ����, ����ֱ�Ӽ򵥵�ɾ��
	if ( (events & (EPOLLIN|EPOLLOUT)) != (EPOLLIN|EPOLLOUT) )
	{
		if ( (events & EPOLLIN) && eventpair->evwrite != NULL )
		{
			// ɾ�����Ƕ��¼�, д�¼���ȻҪ����
			// ���һ���Ҫ��д�¼��в鿴�Ƿ�֧���˱�Ե����ģʽ
			delwrite = 0;
			events = EPOLLOUT;
			op = EPOLL_CTL_MOD;
		}
		else if ( (events & EPOLLOUT) && eventpair->evread != NULL )
		{
			// ɾ������д�¼�, ���¼���ȻҪ����
			// ���һ���Ҫ�Ӷ��¼��в鿴�Ƿ�֧���˱�Ե����ģʽ
			delread = 0;
			events = EPOLLIN;
			op = EPOLL_CTL_MOD;
		}
	}

	epollevent.data.fd = fd;
	epollevent.events = events;

	if ( delread )
	{
		eventpair->evread = NULL;
	}
	if ( delwrite )
	{
		eventpair->evwrite = NULL;
	}

	if ( epoll_ctl( poller->epollfd, op, fd, &epollevent ) == -1 )
	{
		return -2;
	}

	return 0;
}

int32_t epoll_dispatch( struct eventset * sets, void * arg, int32_t tv )
{
	int32_t i, res = -1;
	struct epoller * poller = (struct epoller *)arg;

	if ( tv > MAX_EPOLL_WAIT )
	{
		tv = MAX_EPOLL_WAIT;
	}

	res = epoll_wait( poller->epollfd, poller->events, poller->nevents, tv );
	if ( res == -1 )
	{
		if ( errno != EINTR )
		{
			return -1;
		}

		return 0;
	}

	for ( i = 0; i < res; ++i )
	{
		int32_t fd = poller->events[i].data.fd;
		int32_t what = poller->events[i].events;

		struct eventpair * eventpair = NULL;
		struct event * evread = NULL, * evwrite = NULL;

		if ( fd < 0 || fd >= poller->npairs )
		{
			continue;
		}

		eventpair = &( poller->evpairs[fd] );

		if ( what & (EPOLLHUP|EPOLLERR) )
		{
			evread = eventpair->evread;
			evwrite = eventpair->evwrite;
		}
		else
		{
			if ( what & (EPOLLIN|EPOLLPRI) )
			{
				evread = eventpair->evread;
			}
			if ( what & EPOLLOUT )
			{
				evwrite = eventpair->evwrite;
			}
		}

		if ( !(evread||evwrite) )
		{
			continue;
		}

		if ( evread )
		{
			event_active( evread, EV_READ );
		}
		if ( evwrite )
		{
			event_active( evwrite, EV_WRITE );
		}
	}

	if ( res == poller->nevents )
	{
		epoll_expand( poller );
	}

	return res;
}

void epoll_final( void * arg )
{
	struct epoller * poller = ( struct epoller * )arg;

	if ( poller->evpairs )
	{
		free( poller->evpairs );
	}
	if ( poller->events )
	{
		free( poller->events );
	}
	if ( poller->epollfd >= 0 )
	{
		close( poller->epollfd );
	}

	free( poller );
	return;
}

