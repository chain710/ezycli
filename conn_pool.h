#ifndef _CONN_POOL_H_
#define _CONN_POOL_H_

#include "conn_define.h"

const int MAX_CONN_NUM_IN_POOL = 4;

namespace ezycli
{
    class conn_pool
    {
    public:
        conn_pool();
        ~conn_pool();
        void set_inactive_conn_timeout(int secs);
        int add_conn(const char* ip, unsigned short port);  // 进行异步连接， 立即返回
        conn_with_credit* pick_conn(); // 选择一个已经ok的连接, 这里可能阻塞一会
        void clear_conns();
        void get_conns_msg(char* str, size_t len) const;
    private:
        int async_connect(int idx); // 异步连接
        int setup_conn(int idx, unsigned int ip_addr, unsigned short net_port);
        int clear_conn(int idx);
        int reconnect(int idx);
        int check_conns();
        conn_with_addr conns_[MAX_CONN_NUM_IN_POOL];
        int inactive_conn_timeout_;
    };
}


#endif
