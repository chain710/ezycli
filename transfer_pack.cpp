#include "transfer_pack.h"
#include "conn_define.h"
#include "conn_pool_error.h"
#include "errno.h"
#include <sys/socket.h>

using namespace ezycli;


void ezycli_test_func()
{

}

static bool is_valid_conn(conn_node c)
{
    return c.fd >= 0 && c.status == cst_connected;
}

static bool is_valid_pack(const transfer_pack *pack)
{
    return pack && pack->ptr && pack->length > 0;
}

int ezycli::send_pack( conn_with_credit *cconn, transfer_pack pack, timeval *timeout )
{
    if (NULL == cconn || !is_valid_conn(cconn->conn) || !is_valid_pack(&pack))
    {
        return cp_bad_param;
    }

    cconn->last_active_time = time(NULL);
    size_t wrlen = 0;
    int sendret = 0;
    fd_set wrfds;

    while (wrlen < pack.length)
    {
        if (timeout && timeout->tv_sec <= 0 && timeout->tv_usec <= 0)
        {
            // timeout
            cconn->conn.status = cst_error;
            return cp_timeout;
        }

        FD_ZERO(&wrfds);
        FD_SET(cconn->conn.fd, &wrfds);
        sendret = select(cconn->conn.fd+1, NULL, &wrfds, NULL, timeout);
        if (sendret <= 0)
        {
            if (sendret < 0 || EINTR == errno)
            {
                continue;
            }

            cconn->conn.status = cst_error;
            return cp_sockop_fail;
        }

        if (!FD_ISSET(cconn->conn.fd, &wrfds))
        {
            continue;
        }

        sendret = send(cconn->conn.fd, (const char*)pack.ptr + wrlen, pack.length - wrlen, 0);
        if (sendret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                cconn->conn.status = cst_error;
                return cp_send_fail;
            }
        }
        else
        {
            wrlen += sendret;
        }
    }

    return cp_succ;
}

int ezycli::recv_pack( conn_with_credit *cconn, transfer_pack *pack, pack_checker is_pack, int pack_offset, timeval *timeout )
{
    if (NULL == cconn || !is_valid_conn(cconn->conn) || !is_valid_pack(pack))
    {
        return cp_bad_param;
    }

    cconn->last_active_time = time(NULL);
    size_t rdlen = 0;
    int recvret = 0;
    fd_set rdfds;

    while (rdlen < pack->length)
    {
        if (timeout && timeout->tv_sec <= 0 && timeout->tv_usec <= 0)
        {
            // timeout
            cconn->conn.status = cst_error;
            return cp_timeout;
        }

        FD_ZERO(&rdfds);
        FD_SET(cconn->conn.fd, &rdfds);
        recvret = select(cconn->conn.fd+1, &rdfds, NULL, NULL, timeout);
        if (recvret <= 0)
        {
            if (recvret < 0 && errno == EINTR)
            {
                continue;
            }

            cconn->conn.status = cst_error;
            return cp_sockop_fail;
        }

        if (!FD_ISSET(cconn->conn.fd, &rdfds))
        {
            continue;
        }

        recvret = recv(cconn->conn.fd, (char*)pack->ptr + rdlen, pack->length - rdlen, 0);
        if (recvret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                cconn->conn.status = cst_error;
                return cp_recv_fail;
            }
        }
        else if (0 == recvret)
        {
            // closed by peer
            cconn->conn.status = cst_closed;
            return cp_conn_closed;
        }
        else
        {
            rdlen += recvret;
            if (is_pack((char*)pack->ptr + pack_offset, rdlen - pack_offset))
            {
                pack->length = rdlen;
                return cp_succ;
            }
        }
    }

    return cp_recv_fail;
}
