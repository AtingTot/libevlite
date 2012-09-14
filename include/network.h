
#ifndef NETWORK_H
#define NETWORK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

//
// IO����
//
//		process()	- �յ����ݰ��Ļص�
//						����ֵΪ����������ݰ�, <0: �������
//
//		timeout()	- ��ʱ�Ļص�
//		keepalive()	- ���ʱ����ʱ�Ļص�
//		error()		- ����Ļص�
//		shutdown()	- �Ự��ֹʱ�Ļص�
//						���ط�0, libevlite����ֹ�Ự
//
typedef struct
{
	int32_t (*process)( void * context, const char * buf, uint32_t nbytes );
	int32_t (*timeout)( void * context );
	int32_t (*keepalive)( void * context );
	int32_t (*error)( void * context, int32_t result );
	int32_t (*shutdown)( void * context );
}ioservice_t;

//
// ������
//
typedef uint64_t sid_t;
typedef void * server_t;

// ����������
//		host		- �󶨵ĵ�ַ
//		port		- �����Ķ˿ں�
//		nthreads	- �������ٸ��߳�
//		nclients	- ���֧�ֶ��ٸ��ͻ���
//		cb			- �»Ự�����ɹ���Ļص�,�ᱻ��������̵߳��� 
//							����1: �����Ĳ���; 
//							����2: �»ỰID; 
//							����3: �Ự��IP��ַ; 
//							����4: �Ự�Ķ˿ں�
//		context		- �����Ĳ���
server_t server_start( const char * host, uint16_t port,
							uint8_t nthreads, uint32_t nclients, 
							int32_t (*cb)(void *, sid_t, const char *, uint16_t), void * context );

// �Ự����������, ֻ����ioservice_t��ʹ��
int32_t server_set_timeout( server_t self, sid_t id, int32_t seconds );
int32_t server_set_keepalive( server_t self, sid_t id, int32_t seconds );
int32_t server_set_service( server_t self, sid_t id, ioservice_t * service, void * context );

// �������������ݵ��Ự
int32_t server_send( server_t self, sid_t id, const char * buf, uint32_t nbytes, int32_t iscopy );

// �������㲥���ݵ�ָ���ĻỰ
int32_t server_broadcast( server_t self, 
							sid_t * ids, uint32_t count, 
							const char * buf, uint32_t nbytes, int32_t iscopy );

// ��������ָֹ���ĻỰ
int32_t server_shutdown( server_t self, sid_t id );
int32_t server_shutdowns( server_t self, sid_t * ids, uint32_t count );

// ������ֹͣ
void server_stop( server_t self );

//
// �ͻ���
//
typedef void * client_t;

// �ͻ��˿���
//		host		- Զ�̷�����
//		port		- Զ�̶˿ں�
//		seconds		- ����Զ�̷�������ʱʱ��
//		cb			- ���ӳɹ���Ļص�
//							����1: �����Ĳ���
//							����2: ���ӽ��, 0: �ɹ�, !=0: ʧ��
//		context		- �����Ĳ���
client_t client_start( const char * host, uint16_t port, 
						int32_t seconds, int32_t (*cb)(void *, int32_t), void * context );

// ���������������
int32_t client_send( client_t self, const char * buf, uint32_t nbytes, int32_t iscopy );

// �ͻ��˲���������
int32_t client_set_keepalive( client_t self, int32_t seconds );
int32_t client_set_service( client_t self, ioservice_t * service, void * context );

// �ͻ���ֹͣ
int32_t client_stop( client_t self );

#ifdef __cplusplus
}
#endif

#endif

