
#ifndef IO_H 
#define IO_H

#include <vector>
#include <string>

#include "network.h"

namespace Utils
{

//
// �Ự, ���̰߳�ȫ�� 
//

class IIOService;
class IIOSession
{
public :

	IIOSession() 
		: m_Sid( 0 ),
		  m_Layer( NULL ),
		  m_TimeoutSeconds( 0 ),
		  m_KeepaliveSeconds( 0 )
	{}

	virtual ~IIOSession() 
	{}

public :

	//	
	// �����¼�	
	// ��������߳��б�����
	//

	virtual int32_t	onProcess( const char * buf, uint32_t nbytes ) { return 0; } 
	virtual char *	onTransform( const char * buf, uint32_t & nbytes ) { return const_cast<char *>(buf); }
	virtual int32_t	onTimeout() { return 0; }
	virtual int32_t onKeepalive() { return 0; }
	virtual int32_t onError( int32_t result ) { return 0; }
	virtual int32_t onShutdown() { return 0; }

public :
	
	//	
	// �������߳��жԻỰ�Ĳ���
	//
	
	// ��ȡ�ỰID	
	sid_t id() const;

	// ���ó�ʱ/����ʱ��
	void setTimeout( int32_t seconds );
	void setKeepalive( int32_t seconds );

	// ��������
	int32_t send( const std::string & buffer );
	int32_t send( const char * buffer, uint32_t nbytes, bool isfree = false );

	// �رջỰ	
	int32_t shutdown();

private :

	friend class IIOService;

	// ��ʼ���Ự
	void init( sid_t id, iolayer_t layer );

	// �ڲ��ص�����
	static int32_t	onProcessSession( void * context, const char * buf, uint32_t nbytes );
	static char *	onTransformSession( void * context, const char * buf, uint32_t * nbytes );	
	static int32_t	onTimeoutSession( void * context ); 
	static int32_t	onKeepaliveSession( void * context ); 
	static int32_t	onErrorSession( void * context, int32_t result ); 
	static int32_t	onShutdownSession( void * context ); 

private :

	sid_t		m_Sid;
	iolayer_t	m_Layer;

	int32_t		m_TimeoutSeconds;
	int32_t		m_KeepaliveSeconds;
};

//
// ����ͨ�Ų�
//

class IIOService
{
public :

	IIOService( uint8_t nthreads, uint32_t nclients )
		: m_IOLayer(NULL),
		  m_ThreadsCount( nthreads ),
		  m_SessionsCount( nclients ) 
	{}

	virtual ~IIOService() 
	{}

public :

	// ����/�����¼�
	// ��Ҫ�������Լ�ʵ��
	// �п�����IIOService�Ķ�������߳��б�����
	
	virtual IIOSession * onAccept( sid_t id, const char * host, uint16_t port ) { return NULL; }
	virtual IIOSession * onConnect( sid_t id, const char * host, uint16_t port ) { return NULL; } 

public :

	//
	// �̰߳�ȫ��API
	//
	
	// ��������
	bool start();

	// ֹͣ����
	void stop();
	
	// ����/����
	bool listen( const char * host, uint16_t port );
	bool connect( const char * host, uint16_t port, int32_t seconds );

	// ��������
	int32_t send( sid_t id, const std::string & buffer );
	int32_t send( sid_t id, const char * buffer, uint32_t nbytes, bool isfree = false );

	// �㲥����	
	int32_t broadcast( const std::vector<sid_t> & ids, const std::string & buffer );
	int32_t broadcast( const std::vector<sid_t> & ids, const char * buffer, uint32_t nbytes );	

	// ��ֹ�Ự
	int32_t shutdown( sid_t id );
	int32_t shutdown( const std::vector<sid_t> & ids );

public :

	void attach( sid_t id, IIOSession * session );
	
private :
	
	// �ڲ�����
	static int32_t onAcceptSession( void * context, sid_t id, const char * host, uint16_t port );
	static int32_t onConnectSession( void * context, int32_t result, const char * host, uint16_t port, sid_t id );

private :

	iolayer_t	m_IOLayer;

	uint8_t		m_ThreadsCount;
	uint32_t	m_SessionsCount;
};

}

#endif

