
#ifndef NETWORK_H
#define NETWORK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

//
// ����� 
//
typedef uint64_t	sid_t;
typedef void *		iolayer_t;

//
// IO����
//		start()		- �Ự��ʼ�Ļص�
//		process()	- �յ����ݰ��Ļص�
//						����ֵΪ����������ݰ�, <0: �������
//		transform()	- �������ݰ�ǰ�Ļص�
//						������Ҫ���͵����ݰ�, ȷ�����ݰ���malloc()������
//		keepalive()	- ���ʱ����ʱ�Ļص�
//
//		timeout()	- ��ʱ�Ļص�
//		error()		- ����Ļص�
//						����accept()�����Ŀͻ���, ֱ�ӻص�shutdown();
//						����connect()��ȥ�Ŀͻ���, ==0, ��������, !=0, ֱ�ӻص�shutdown() .
//
//		shutdown()	- �Ự��ֹʱ�Ļص�, ���۷���ֵ, ֱ�����ٻỰ
//
typedef struct
{
	int32_t (*start)( void * context );
	int32_t (*process)( void * context, const char * buf, uint32_t nbytes );
	char *	(*transform)( void * context, const char * buf, uint32_t * nbytes );
	int32_t (*keepalive)( void * context );
	int32_t (*timeout)( void * context );
	int32_t (*error)( void * context, int32_t result );
	int32_t (*shutdown)( void * context );
}ioservice_t;

// ��������� 
iolayer_t iolayer_create( uint8_t nthreads, uint32_t nclients );

// ����������
//		host		- �󶨵ĵ�ַ
//		port		- �����Ķ˿ں�
//		cb			- �»Ự�����ɹ���Ļص�,�ᱻ��������̵߳��� 
//							����1: �����Ĳ���; 
//							����2: �»ỰID; 
//							����3: �Ự��IP��ַ; 
//							����4: �Ự�Ķ˿ں�
//		context		- �����Ĳ���
int32_t iolayer_listen( iolayer_t self,
		const char * host, uint16_t port, 
		int32_t (*cb)( void *, sid_t, const char * , uint16_t ), void * context );

// �ͻ��˿���
//		host		- Զ�̷������ĵ�ַ
//		port		- Զ�̷������Ķ˿�
//		seconds		- ���ӳ�ʱʱ��
//		cb			- ���ӽ���Ļص�
//							����1: �����Ĳ���
//							����2: ���ӽ��
//							����3: ���ӵ�Զ�̷������ĵ�ַ
//							����4: ���ӵ�Զ�̷������Ķ˿�
//							����5: ���ӳɹ��󷵻صĻỰID
//		context		- �����Ĳ���
int32_t iolayer_connect( iolayer_t self, 
		const char * host, uint16_t port, int32_t seconds, 
		int32_t (*cb)( void *, int32_t, const char *, uint16_t, sid_t), void * context );

// �Ự����������, ֻ����ioservice_t��ʹ��
int32_t iolayer_set_timeout( iolayer_t self, sid_t id, int32_t seconds );
int32_t iolayer_set_keepalive( iolayer_t self, sid_t id, int32_t seconds );
int32_t iolayer_set_service( iolayer_t self, sid_t id, ioservice_t * service, void * context );

// �������ݵ��Ự
int32_t iolayer_send( iolayer_t self, sid_t id, const char * buf, uint32_t nbytes, int32_t isfree );

// �㲥���ݵ�ָ���ĻỰ
int32_t iolayer_broadcast( iolayer_t self, sid_t * ids, uint32_t count, const char * buf, uint32_t nbytes );

// ��ָֹ���ĻỰ
int32_t iolayer_shutdown( iolayer_t self, sid_t id );
int32_t iolayer_shutdowns( iolayer_t self, sid_t * ids, uint32_t count );

// ���������
void iolayer_destroy( iolayer_t self );

#ifdef __cplusplus
}
#endif

#endif

