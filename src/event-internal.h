
#ifndef EVENT_INTERNAL_H
#define EVENT_INTERNAL_H

#include "queue.h"
#include "event.h"

//
// �¼���״̬
// 0000 0000 0000 0000
// ���� ˽�� ϵͳ ����
//
// ����: 	1 - ��ȫ��������
//			2 - �ڶ�ʱ����
//			3 - ����������
//			4 - ����
//

#define EVSTATUS_INSERTED	0x01
#define EVSTATUS_TIMER		0x02
#define EVSTATUS_ACTIVE	    0x04
#define EVSTATUS_INTERNAL	0x10
#define EVSTATUS_INIT	    0x20

#define EVSTATUS_ALL	    (0x0f00|0x37)

//
//
//
struct event;
struct eventset;
struct eventop
{
    void * (*init)();
    int32_t (*add)(void *, struct event *);
    int32_t (*del)(void *, struct event *);
    int32_t (*dispatch)(struct eventset *, void *, int32_t);
    void (*final)(void *);
};

//
// �¼�ģ��
//

struct event
{
    TAILQ_ENTRY(event) timerlink;
    TAILQ_ENTRY(event) eventlink;
    TAILQ_ENTRY(event) activelink;
    
    int32_t fd;
    int16_t events;

    void * evsets;

    // cb һ��Ҫ�Ϸ�
    void * arg;
    void (*cb)( int32_t, int16_t, void * ); 

    // ��ʱ���ĳ�ʱʱ��
    int32_t timer_msecs;        

    // �¼��ڶ�ʱ�������е�����
    // ɾ��ʱ, ���ٶ�λ��ĳһ��Ͱ
    int32_t timer_index; 

    // �¼���������
    int32_t timer_stepcnt;     

    int32_t status;
    int32_t results;
};

TAILQ_HEAD( event_list, event );

#define EVENT_TIMEOUT(ev)       (int32_t)( (ev)->timer_msecs )
#define EVENT_TIMERINDEX(ev)    (int32_t)( (ev)->timer_index )
#define EVENT_TIMERSTEP(ev)     (int32_t)( (ev)->timer_stepcnt )

inline int32_t event_active( struct event * self, int16_t res );


//
// event��ʱ��ģ��
//

#define TIMER_MAX_PRECISION 10			// ��ʱ����󾫶�Ϊ10ms
#define TIMER_BUCKET_COUNT  4096		// ������2��N�η� 

struct evtimer
{
    int32_t event_count;                // ������¼�����
    int32_t bucket_count;               // Ͱ�ĸ���
    int32_t max_precision;              // ��󾫶�, ��ȷ��1����

    int32_t dispatch_refer;             //
    struct event_list * bucket_array;   // Ͱ������
};

#define EVTIMER_INDEX(t,c) 				( (c) & ((t)->bucket_count-1) )

struct evtimer * evtimer_create( int32_t max_precision, int32_t bucket_count );
int32_t evtimer_append( struct evtimer * self, struct event * ev );
int32_t evtimer_remove( struct evtimer * self, struct event * ev );
int32_t evtimer_dispatch( struct evtimer * self );
int32_t evtimer_count( struct evtimer * self );
int32_t evtimer_clean( struct evtimer * self );
void evtimer_destroy( struct evtimer * self );

//
// �¼���ģ��
//

struct eventset
{
    int32_t timer_precision;
    
    int64_t expire_time;
    struct evtimer * core_timer;

    void * evsets;
    struct eventop * evselect;

    struct event_list eventlist;
    struct event_list activelist;
};

#endif

