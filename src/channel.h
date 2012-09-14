
#ifndef SRC_CHANNEL_H
#define SRC_CHANNEL_H

/*
 * channel ����ͨ��
 * �ṩ����session��һ���������
 */

#include <stdint.h>
#include "session.h"

//
int32_t channel_send( struct session * session, char * buf, uint32_t nbytes );

// �Ự����
// �������Ͷ����е�����
int32_t channel_error( struct session * session, int32_t result );

// �Ự�ر�
// �������Ͷ����е�����
int32_t channel_shutdown( struct session * session );

// �¼��Ļص���������
void channel_on_read( int32_t fd, int16_t ev, void * arg );
void channel_on_write( int32_t fd, int16_t ev, void * arg );
void channel_on_accept( int32_t fd, int16_t ev, void * arg );
void channel_on_connect( int32_t fd, int16_t ev, void * arg );
void channel_on_reconnect( int32_t fd, int16_t ev, void * arg );
void channel_on_keepalive( int32_t fd, int16_t ev, void * arg );

#endif

