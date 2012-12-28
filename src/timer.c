
#include <stdio.h>
#include <stdlib.h>

#include "event-internal.h"

struct evtimer * evtimer_create( int32_t max_precision, int32_t bucket_count )
{
	struct evtimer * t = NULL;

	t = (struct evtimer *)malloc( sizeof(struct evtimer) );
	if ( t )
	{
		t->event_count = 0;
		t->dispatch_refer = 0;
		t->bucket_count = bucket_count;
		t->max_precision = max_precision;

		t->bucket_array = (struct event_list *)malloc( bucket_count * sizeof(struct event_list) );
		if ( t->bucket_array == NULL )
		{
			free( t );
			t = NULL;
		}
		else
		{
			int32_t i = 0;

			for ( i = 0; i < bucket_count; ++i )
			{
				TAILQ_INIT( &(t->bucket_array[i]) );
			}
		}
	}

	return t;
}

int32_t evtimer_append( struct evtimer * self, struct event * ev )
{
	int32_t index = -1;
	int32_t tv = EVENT_TIMEOUT(ev);

	if ( tv <= 0 )
	{
		return -1;
	}

	// ��Ͱ��������д���¼������, ���ڲ����Լ�ɾ��
	// �����ʱ����ʱʱ�����, �趨�䶨ʱ��������
	index = EVTIMER_INDEX(self, tv/self->max_precision+self->dispatch_refer+1);

	ev->timer_index = index;
	ev->timer_stepcnt = tv / ( self->max_precision * self->bucket_count ) + 1;

	++self->event_count;
	TAILQ_INSERT_TAIL( &(self->bucket_array[index]), ev, timerlink );

	return 0;
}

int32_t evtimer_remove( struct evtimer * self, struct event * ev )
{
	// ���ݾ���е�������, ���ٶ�λͰ
	int32_t index = EVENT_TIMERINDEX(ev);

	if ( index < 0 || index >= self->bucket_count )
	{
		return -1;
	}

	ev->timer_index = -1;
	ev->timer_stepcnt = -1;

	--self->event_count;
	TAILQ_REMOVE( &(self->bucket_array[index]), ev, timerlink );

	return 0;
}

int32_t evtimer_dispatch( struct evtimer * self )
{
	int32_t rc = 0;
	int32_t done = 0, index = 0;

	struct event * laster = NULL;
	struct event_list * head = NULL;

	index = EVTIMER_INDEX(self, self->dispatch_refer);
	++self->dispatch_refer;
	head = &( self->bucket_array[index] );

	// ��Ͱ��û�ж�ʱ���¼�
	if ( TAILQ_EMPTY(head) )
	{
		return 0;
	}

	// ������ʱ�¼�����
	laster = TAILQ_LAST( head, event_list );
	while ( !done )
	{
		struct event * ev = TAILQ_FIRST(head);

		// ����ĳЩ�¼��ĳ�ʱʱ�����
		// ���Ի�����Ҫ������ӵ��¼������е�
		// �����жϵ�ǰ�ڵ��Ƿ��������β��Ԫ��
		if ( ev == laster )
		{
			done = 1;
		}

		--ev->timer_stepcnt;
		if ( ev->timer_stepcnt > 0 )
		{
			// δ��ʱ
			TAILQ_REMOVE( head, ev, timerlink );
			TAILQ_INSERT_TAIL( head, ev, timerlink );
		}
		else if ( ev->timer_stepcnt == 0 )
		{
			// ��ʱ��
			// �Ӷ�����ɾ��, ����ӵ����������
			++rc;
			evsets_del( event_get_sets((event_t)ev), (event_t)ev );
			event_active( ev, EV_TIMEOUT );
		}
		else
		{
			// ������
			evsets_del( event_get_sets((event_t)ev), (event_t)ev );
		}
	}

	return rc;
}

int32_t evtimer_count( struct evtimer * self )
{
	return self->event_count;
}

int32_t evtimer_clean( struct evtimer * self )
{
	int32_t i = 0, rc = 0;

	for ( i = 0; i < self->bucket_count; ++i )
	{
		struct event * ev = NULL;
		struct event_list * head = &( self->bucket_array[i] );

		for ( ev = TAILQ_FIRST(head); ev; )
		{
			struct event * next = TAILQ_NEXT( ev, timerlink );

			evsets_del( event_get_sets((event_t)ev), (event_t)ev );

			++rc;
			ev = next;
			--self->event_count;
		}
	}

	self->dispatch_refer = 0;

	return rc;
}

void evtimer_destroy( struct evtimer * self )
{
	if ( self->bucket_array )
	{
		free( self->bucket_array );
	}

	free( self );

	return;
}

