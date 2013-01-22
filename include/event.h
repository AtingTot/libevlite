
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

#ifndef EVENT_H
#define EVENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

//
// �¼�����֧�ֵ��¼�����
//

#define EV_READ		0x01    // ���¼�
#define EV_WRITE	0x02    // д�¼�
#define EV_TIMEOUT	0x04    // ��ʱ�¼�
#define EV_PERSIST	0x08    // ����ģʽ


//
// �¼��Ķ���, �Լ��¼����Ķ���
//

typedef void * event_t;
typedef void * evsets_t;

//
// �¼��ķ���
//

// �����¼�
event_t event_create();

// �����¼���һЩ��������
// fd - ��ע��������; ev - ��ע���¼�,�����϶����������
void event_set( event_t self, int32_t fd, int16_t ev );

// �����¼��Ļص�����
// ���÷����¼���Ļص�����
void event_set_callback( event_t self, void (*cb)(int32_t, int16_t, void *), void * arg );

// ��ȡ�¼���ע��������FD
int32_t event_get_fd( event_t self );

// ��ȡ�¼������¼���
evsets_t event_get_sets( event_t self );

// �����¼�
void event_destroy( event_t self );

// 
// �¼����ķ���
//

// �����¼���
evsets_t evsets_create();

// �¼���İ汾
const char * evsets_get_version();

// ���¼���������¼�
// ����0, ����ָ�����¼��ɹ�����ӵ��¼�����
// ����1, ����ָ�����¼��Ƿ�, û����ӵ��¼�����
// ����<0, ����¼�ʧ��
int32_t evsets_add( evsets_t self, event_t ev, int32_t tv );

// ���¼�����ɾ���¼�
int32_t evsets_del( evsets_t self, event_t ev );

// �ַ��������¼�
// ���ؼ�����¼�����
int32_t evsets_dispatch( evsets_t self );

// �����¼���
void evsets_destroy( evsets_t self );

#ifdef __cplusplus
}
#endif

#endif

