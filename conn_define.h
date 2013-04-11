#ifndef _EZYCLI_CONN_DEFINE_H_
#define _EZYCLI_CONN_DEFINE_H_
#include <netinet/in.h>
#include <time.h>
#include <sys/types.h>

namespace ezycli
{
    enum
    {
        // conn status
        cst_error = -1,
        cst_closed = 0,
        cst_opened, 
        cst_connecting,
        cst_connected,
    };

    struct conn_node
    {
        int fd;
        int status;
    };

    struct transfer_pack
    {
        const void* ptr;
        size_t length;
    };

    struct conn_with_credit
    {
        conn_node conn;
        int credits;
        time_t last_prob_time;  // 最后一次探测时间(用于质量比较差的链路)
        time_t last_active_time;    // 最后一次活跃时间(收发包)
    };

    struct conn_with_addr
    {
        conn_with_credit cconn;
        sockaddr_in remote_addr;
        char used;
    };
}

#endif
