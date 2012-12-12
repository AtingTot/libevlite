
#ifndef SRC_MESSAGE_H
#define SRC_MESSAGE_H

#include <stdint.h>

#include "utils.h"

//
// ������
//
struct buffer
{
	uint32_t offset;		// ��Ч���ݶ������ԭʼ���ݶε�ƫ����
	uint32_t length;		// ��Ч���ݶεĳ���
	uint32_t totallen;		// �ڴ����ܳ���

	char * buffer;			// ��Ч���ݶ�
	char * orignbuffer;		// ԭʼ���ݶ�
};

// ���û�����
// �ٶȿ�, �������ڴ�copy, bufһ����malloc()�������ڴ��ַ
int32_t buffer_set( struct buffer * self, char * buf, uint32_t length );

// ��ȡ���绺�����ô�С������
uint32_t buffer_length( struct buffer * self );
char * buffer_data( struct buffer * self );

//
int32_t buffer_erase( struct buffer * self, uint32_t length );
int32_t buffer_append( struct buffer * self, char * buf, uint32_t length );
uint32_t buffer_take( struct buffer * self, char * buf, uint32_t length );

// �����������໥����
void buffer_exchange( struct buffer * buf1, struct buffer * buf2 );

// -1, ϵͳ����read()���س���; -2, ����expand()ʧ��
int32_t buffer_read( struct buffer * self, int32_t fd, int32_t nbytes );


//
// ��������
//

// TODO: �Ƿ��б�Ҫдһ����������������л������ķ������ͷ�

//
// ��Ϣ
//
struct message
{
    int32_t nsuccess;

    struct sidlist * tolist;
    struct sidlist * failurelist;
    
    struct buffer 	buffer;
};

// ����/���� ��Ϣ
struct message * message_create();
void message_destroy( struct message * self );

// ������Ϣ�Ľ�����
// ������Ϣ�Ľ����б�
int32_t message_add_receiver( struct message * self, sid_t id );
int32_t message_set_receivers( struct message * self, struct sidlist * ids );

//
int32_t message_add_failure( struct message * self, sid_t id );

// ���/���� ��Ϣ������
#define message_set_buffer( self, buf, nbytes ) 	buffer_set( &((self)->buffer), (buf), (nbytes) )
#define message_add_buffer( self, buf, nbytes ) 	buffer_append( &((self)->buffer), (buf), (nbytes) )

// ��Ϣ�Ƿ���ȫ����
int32_t message_left_count( struct message * self );
#define message_is_complete( self ) 				message_left_count( (self) )

// ��ȡ��Ϣ���ݵĳ����Լ���
#define message_get_buffer( self )					buffer_data( &((self)->buffer) )
#define message_get_length( self ) 					buffer_length( &((self)->buffer) )

#endif

