
#ifndef SRC_NETWORK_INTERNAL_H
#define SRC_NETWORK_INTERNAL_H

#include <stdint.h>

// �Ƿ�ȫ����ֹ�Ự
#define SAFE_SHUTDOWN					0	

// ���Ͷ��е�Ĭ�ϴ�С
#define DEFAULT_SENDQUEUE_SIZE			128

// �ر�ǰ���ȴ�ʱ��,Ĭ��10s
#define MAX_SECONDS_WAIT_FOR_SHUTDOWN	(10*1000)	

// ���������ļ��ʱ��,Ĭ��Ϊ100ms
#define TRY_RECONNECT_INTERVAL			100

// ���ͽ��ջ���������
#define SEND_BUFFER_SIZE				0
#define RECV_BUFFER_SIZE				4096


// ��������
enum
{
	eIOTaskType_Invalid		= 0,
	eIOTaskType_Listen		= 1,
	eIOTaskType_Assign		= 2,
	eIOTaskType_Connect		= 3,
	eIOTaskType_Send		= 4,
	eIOTaskType_Broadcast	= 5,
	eIOTaskType_Shutdown	= 6,
	eIOTaskType_Shutdowns	= 7,	
};

// �����������붨��
enum
{
	eIOError_OutMemory 			= 0x00010001,
	eIOError_ConnectStatus		= 0x00010002,	// �Ƿ�������״̬
	eIOError_ConflictSid		= 0x00010003,	// ��ͻ��SID
	eIOError_InBufferFull		= 0x00010004,	// ����������
	eIOError_ReadFailure		= 0x00010005,	// read()ʧ��
	eIOError_PeerShutdown		= 0x00010006,	// �Զ˹ر�������
	eIOError_WriteFailure		= 0x00010007, 	// write()ʧ��
	eIOError_ConnectFailure		= 0x00010008,	// ����ʧ��
	eIOError_Timeout			= 0x00010009,	// ���ӳ�ʱ��
};


#endif

