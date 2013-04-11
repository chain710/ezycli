#include "conn_pool.h"
#include "conn_pool_error.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <cstdio>

using namespace ezycli;

int setnonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        return -1;
    }

    return 0;
}

static bool is_valid_fd(int fd)
{
    int error;
    socklen_t len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
    {
        return false;
    }

    return error == 0;
}

conn_pool::conn_pool()
{
    memset(&conns_[0], 0, sizeof(conns_));
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        conns_[i].cconn.conn.fd = -1;
    }
}

conn_pool::~conn_pool()
{
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        clear_conn(i);
    }
}

int conn_pool::add_conn( const char* ip, unsigned short port )
{
    unsigned int ip_addr = inet_addr(ip);
    unsigned short net_port = htons(port);
    int avaiable_slot = -1;
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        if ( avaiable_slot < 0 && conns_[i].used == 0 )
        {
            avaiable_slot = i;
            // dont break here in case there's already a connected fd
        }

        if (conns_[i].remote_addr.sin_addr.s_addr == ip_addr 
            && conns_[i].remote_addr.sin_port == net_port)
        {
            if (conns_[i].cconn.conn.fd >= 0 && cst_connected == conns_[i].cconn.conn.status)
            {
                // already connected
                return cp_succ;
            }

            // same addr, but not connected yet
            avaiable_slot = i;
            break;
        }
    }

    if (avaiable_slot < 0)
    {
        return cp_succ;
    }

    int ret = clear_conn(avaiable_slot);
    if (ret) return ret;
    // 创建socket
    ret = setup_conn(avaiable_slot, ip_addr, net_port);
    if (ret) return ret;
    // 启动连接
    return async_connect(avaiable_slot);
}

conn_with_credit* conn_pool::pick_conn()
{
    int conning_slot_num = 0;
    int conning_slot[MAX_CONN_NUM_IN_POOL] = {0};

    int conned_slot_num = 0;
    int conned_slot[MAX_CONN_NUM_IN_POOL] = {0};

    int ret = 0;
    ret = check_conns();
    if (ret < 0)
    {
        return NULL;
    }

    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        if (0 == conns_[i].used)
        {
            continue;
        }

        if (conns_[i].cconn.conn.status == cst_connecting)
        {
            conning_slot[conning_slot_num++] = i;
        }
        else if (conns_[i].cconn.conn.status == cst_connected)
        {
            conned_slot[conned_slot_num++] = i;
        }
        else
        {
            // do nothing
        }
    }

    if (0 == conned_slot_num && conning_slot_num > 0)
    {
        // wait one connected till timeout, 1s
        fd_set wrfds;
        FD_ZERO(&wrfds);
        int maxfd = 0;
        for (int i = 0; i < conning_slot_num; ++i)
        {
            FD_SET(conns_[conning_slot[i]].cconn.conn.fd, &wrfds);
            if (conns_[conning_slot[i]].cconn.conn.fd > maxfd)
            {
                maxfd = conns_[conning_slot[i]].cconn.conn.fd;
            }
        }
        
        timeval timeout = {1, 0};
        int sel = select(maxfd + 1, NULL, &wrfds, NULL, &timeout);
        if (sel > 0)
        {
            for (int i = 0; i < conning_slot_num; ++i)
            {
                if (FD_ISSET(conns_[conning_slot[i]].cconn.conn.fd, &wrfds))
                {
                    if (is_valid_fd(conns_[conning_slot[i]].cconn.conn.fd))
                    {
                        conns_[conning_slot[i]].cconn.conn.status = cst_connected;
                        conned_slot[conned_slot_num++] = conning_slot[i];
                    }
                    else
                    {
                        conns_[conning_slot[i]].cconn.conn.status = cst_error;
                    }
                }
            }
        }
        else
        {
            return NULL;
        }
    }

    if (conned_slot_num <= 0)
    {
        // 没有已连上的
        return NULL;
    }

    // find one connected
    // simple policy this version, just random pick
    int slot_idx = conned_slot[rand() % conned_slot_num];
    return &conns_[slot_idx].cconn;
}

int conn_pool::async_connect( int idx )
{
    assert(idx >= 0 && idx < MAX_CONN_NUM_IN_POOL);
    if (conns_[idx].cconn.conn.status == cst_connecting)
    {
        // select if it's connected?
        fd_set wrfds;
        FD_ZERO(&wrfds);
        FD_SET(conns_[idx].cconn.conn.fd, &wrfds);
        timeval timeout = {0, 0};
        int ret = select(conns_[idx].cconn.conn.fd + 1, NULL, &wrfds, NULL, &timeout);
        if (ret > 0 && (FD_ISSET(conns_[idx].cconn.conn.fd, &wrfds)))
        {
            conns_[idx].cconn.conn.status = is_valid_fd(conns_[idx].cconn.conn.fd)? cst_connected: cst_error;
        }

        return cp_succ;
    }
    else if (conns_[idx].cconn.conn.status == cst_connected)
    {
        // already connected!
        return cp_succ;
    }
    else
    {
        int ret = connect(conns_[idx].cconn.conn.fd, (sockaddr*)&conns_[idx].remote_addr, sizeof(conns_[idx].remote_addr));
        if (0 == ret)
        {
            // 一般客户端和服务端在同一台机器会立刻连接完成
            conns_[idx].cconn.conn.status = cst_connected;
        }
        else
        {
            if (errno == EINPROGRESS)
            {
                conns_[idx].cconn.conn.status = cst_connecting;
            }
            else
            {
                conns_[idx].cconn.conn.status = cst_error;
                return cp_conn_fail;
            }
        }
        
    }

    return cp_succ;
}

int conn_pool::setup_conn( int idx, unsigned int ip_addr, unsigned short net_port )
{
    if (idx < 0 || idx >= MAX_CONN_NUM_IN_POOL)
    {
        return cp_bad_param;
    }

    int newfd = socket(AF_INET, SOCK_STREAM, 0);
    if (newfd < 0)
    {
        return cp_create_sock_fail;
    }

    // TODO: socket重用, nonblock
    setnonblock(newfd);

    memset(&conns_[idx], 0, sizeof(conns_[idx]));
    conns_[idx].used = 1;
    conns_[idx].remote_addr.sin_family = AF_INET;
    conns_[idx].remote_addr.sin_port = net_port;
    conns_[idx].remote_addr.sin_addr.s_addr = ip_addr;
    conns_[idx].cconn.last_prob_time = time(NULL);
    conns_[idx].cconn.last_active_time = conns_[idx].cconn.last_prob_time;
    conns_[idx].cconn.conn.status = cst_opened;
    conns_[idx].cconn.conn.fd = newfd;

    return cp_succ;
}

int conn_pool::clear_conn( int idx )
{
    if (idx < 0 || idx >= MAX_CONN_NUM_IN_POOL)
    {
        return cp_bad_param;
    }

    if (conns_[idx].cconn.conn.fd >= 0)
    {
        close(conns_[idx].cconn.conn.fd);
        memset(&conns_[idx], 0, sizeof(conns_[idx]));
        conns_[idx].cconn.conn.fd = -1;
        conns_[idx].cconn.conn.status = cst_closed;
    }

    return cp_succ;
}

int conn_pool::reconnect( int idx )
{
    int ret;
    sockaddr_in addr = conns_[idx].remote_addr;
    ret = clear_conn(idx);
    if (ret) return ret;
    ret = setup_conn(idx, 
        addr.sin_addr.s_addr, 
        addr.sin_port);
    if (ret) return ret;
    ret = async_connect(idx);
    return ret;
}

void conn_pool::set_inactive_conn_timeout( int secs )
{
    inactive_conn_timeout_ = secs;
}

int ezycli::conn_pool::check_conns()
{
    // select 所有可读/错误的fd
    // 有错误直接重连
    // 可读的话，判断接收缓冲区大小，如果为0表示对端关闭连接，重连
    time_t cur_time = time(NULL);
    fd_set rdfds, errfds;
    FD_ZERO(&rdfds);
    FD_ZERO(&errfds);
    timeval timeout = {0, 0};
    int maxfd = -1;
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        if (0 == conns_[i].used)
        {
            continue;
        }

        if (conns_[i].cconn.conn.status == cst_connecting)
        {
            // 连接超时
            if (cur_time - conns_[i].cconn.last_active_time > inactive_conn_timeout_)
            {
                reconnect(i);
            }
        }
        else if (conns_[i].cconn.conn.status == cst_connected)
        {
            if (cur_time - conns_[i].cconn.last_active_time > inactive_conn_timeout_)
            {
                // 太久没有包了
                reconnect(i);
            }
        }
        else
        {
            // 其他非连接状态
            reconnect(i);
        }

        FD_SET(conns_[i].cconn.conn.fd, &rdfds);
        FD_SET(conns_[i].cconn.conn.fd, &errfds);
        if (conns_[i].cconn.conn.fd > maxfd)
        {
            maxfd = conns_[i].cconn.conn.fd;
        }
    }

    int ret;
    ret = select(maxfd + 1, &rdfds, NULL, &errfds, &timeout);
    if (ret < 0)
    {
        return cp_send_fail;
    }

    int rdbuf_len = 0;
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        if (conns_[i].cconn.conn.fd < 0 || 0 == conns_[i].used)
        {
            continue;
        }

        if (FD_ISSET(conns_[i].cconn.conn.fd, &rdfds))
        {
            // 判断接受缓冲区大小
            ret = ioctl(conns_[i].cconn.conn.fd, FIONREAD, &rdbuf_len);
            if (0 == ret && 0 == rdbuf_len)
            {
                // 对方断开连接
                reconnect(i);
            }
        }

        if (FD_ISSET(conns_[i].cconn.conn.fd, &errfds))
        {
            // 发生错误的socket，重连
            reconnect(i);
        }
    }

    return cp_succ;
}

void ezycli::conn_pool::clear_conns()
{
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        clear_conn(i);
    }
}

void ezycli::conn_pool::get_conns_msg( char* str, size_t len ) const
{
    int ret;
    int offset = 0;
    for (int i = 0; i < MAX_CONN_NUM_IN_POOL; ++i)
    {
        ret = snprintf(str + offset, len - offset,  "c[%d] used=%d fd=%d status=%d la=%u,", 
            i, 
            conns_[i].used, 
            conns_[i].cconn.conn.fd, 
            conns_[i].cconn.conn.status, 
            (unsigned int)conns_[i].cconn.last_active_time);
        if (ret <= 0) return;
        offset += ret;
    }
}
