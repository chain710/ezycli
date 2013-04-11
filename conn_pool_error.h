#ifndef _CONN_POOL_ERROR_
#define _CONN_POOL_ERROR_

namespace ezycli
{
    enum
    {
        cp_succ = 0,
        cp_bad_param = 0x010001,
        cp_create_sock_fail = 0x010002,
        cp_timeout = 0x010003,
        cp_sockop_fail = 0x010004,
        cp_recv_fail = 0x010005,
        cp_send_fail = 0x010005,
        cp_conn_closed = 0x010006,
        cp_conn_fail = 0x010007,
    };
}

#endif
