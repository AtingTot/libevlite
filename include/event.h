
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

