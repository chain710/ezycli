#ifndef _EZYCLI_TRANSFER_PACK_
#define _EZYCLI_TRANSFER_PACK_
#include "conn_define.h"

namespace ezycli
{

typedef bool (*pack_checker)(const void* ptr, size_t length);

//************************************
// Method:    send_pack ����pack
// FullName:  send_pack
// Access:    public 
// Returns:   int
// Qualifier:
// Parameter: conn_with_credit * conn
// Parameter: transfer_pack pack
// Parameter: timeval * timeout
//************************************
int send_pack(conn_with_credit *conn, transfer_pack pack, timeval *timeout);

//************************************
// Method:    recv_pack ����pack
// FullName:  recv_pack
// Access:    public 
// Returns:   int
// Qualifier:
// Parameter: conn_with_credit * cconn
// Parameter: transfer_pack pack[���ػ��޸�length,ָʾʵ���յ��ĳ���]
// Parameter: pack_checker is_pack
// Parameter: int pack_offset
// Parameter: timeval * timeout
//************************************
int recv_pack(conn_with_credit *cconn, transfer_pack *pack, pack_checker is_pack, int pack_offset, timeval *timeout);

} // end of namespace

#ifdef __cplusplus
extern "C" {
#endif
    void ezycli_test_func();
#ifdef __cplusplus
}
#endif

#endif
