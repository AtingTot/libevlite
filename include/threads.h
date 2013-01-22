
/*
 * Copyright (c) 2012, Raymond Zhang <spriteray@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

