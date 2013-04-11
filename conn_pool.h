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
        int add_conn(const char* ip, unsigned short port);  // �����첽���ӣ� ��������
        conn_with_credit* pick_conn(); // ѡ��һ���Ѿ�ok������, �����������һ��
        void clear_conns();
        void get_conns_msg(char* str, size_t len) const;
    private:
        int async_connect(int idx); // �첽����
        int setup_conn(int idx, unsigned int ip_addr, unsigned short net_port);
        int clear_conn(int idx);
        int reconnect(int idx);
        int check_conns();
        conn_with_addr conns_[MAX_CONN_NUM_IN_POOL];
        int inactive_conn_timeout_;
    };
}


#endif
