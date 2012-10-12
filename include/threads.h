
#ifndef THREADS_H
#define THREADS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <pthread.h>

#include "event.h"

// �����߳���
typedef void * iothreads_t;

// ���������߳���
// nthreads		- �����߳����е��߳���
// method		- ��������
iothreads_t iothreads_start( uint8_t nthreads, 
					void (*method)(void *, uint8_t, int16_t, void *), void * context );

// ��ȡ�����߳�����ָ���̵߳�ID
pthread_t iothreads_get_id( iothreads_t self, uint8_t index );

// ��ȡ�����߳�����ָ���̵߳��¼���
evsets_t iothreads_get_sets( iothreads_t self, uint8_t index );

// �������߳�����ָ�����߳��ύ����
// index	- ָ�������̵߳ı��
// type		- �ύ����������, NOTE:0xff���õ���������
// task		- �ύ����������
// size		- �������ݵĳ���, Ĭ������Ϊ0
int32_t iothreads_post( iothreads_t self, uint8_t index, int16_t type, void * task, uint8_t size );

// �����߳���ֹͣ
void iothreads_stop( iothreads_t self );

#ifdef __cplusplus
}
#endif

#endif

