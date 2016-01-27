#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

#include "melo_udp.h"

// Author: Liigo <com.liigo@gmail.com>.

static void _cb_on_udp_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned int flags);
static void _cb_after_udp_send(uv_udp_send_t* req, int status);


void melo_udp_init(melo_udp_t * mudp)
{
    MELO_ASSERT(mudp != NULL, "xudp is NULL");
    memset(mudp, 0, sizeof(melo_udp_t));
}

void melo_udp_reg_listener(melo_udp_t * mudp, melo_udp_listener_t type, void * listener)
{
    MELO_ASSERT(mudp != NULL, "xudp is NULL");
    MELO_ASSERT(type < MELO_U_ON_MAX, "invalid type");

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) case id: mudp->func = listener; break;

    switch (type)
    {
        MELO_U_CBs

        default:
            break;
    }
}


melo_udp_config_t melo_udp_default_config(melo_udp_t * mudp)
{
    melo_udp_config_t config = { 0 };
    snprintf(config.name, sizeof(config.name), "xudp-%p", mudp);
    config.log_out = stdout;
    config.log_err = stderr;
    return config;
}

int melo_udp_start(melo_udp_t * mudp, uv_loop_t* loop, melo_udp_config_t config)
{
    assert(mudp && loop);
    mudp->uvloop = loop;
    memcpy(&mudp->config, &config, sizeof(melo_udp_config_t));

    // init udp
    uv_udp_init(loop, &mudp->uvudp);
    mudp->uvudp.data = mudp;

    if (mudp->config.ip)
    {
        int r;
        melo_sockaddr_t sockaddr;
        melo_get_sock_addr(mudp->config.ip, mudp->config.port, &sockaddr);
        r = uv_udp_bind(&mudp->uvudp, (const struct sockaddr *)&sockaddr, 0);

        if(r >= 0)
        {
            char timestr[32]; time_t t; time(&t);
            strftime(timestr, sizeof(timestr), "[%Y-%m-%d %X]", localtime(&t)); // C99 only: %F = %Y-%m-%d
            MELO_INFO(config.log_out,
                    "[uvx-udp] %s %s bind on %s:%d ...\n",
                    timestr, mudp->config.name, mudp->config.ip, mudp->config.port);
        }
        else
        {
            MELO_ERR(config.log_err,
                    "\n!!! [uvx-udp] %s bind on %s:%d failed: %s\n",
                    mudp->config.name, mudp->config.ip, mudp->config.port, uv_strerror(r));
            uv_close((uv_handle_t*) &mudp->uvudp, NULL);
            return ERROR;
        }
    }
    else
    {
        // if ip == NULL, default bind to local random port number (for UDP client)
        MELO_INFO(config.log_out,
                "[uvx-udp] %s bind on local random port ...\n",
                mudp->config.name);
    }

    uv_udp_recv_start(&mudp->uvudp, melo_alloc_buf, _cb_on_udp_recv);

    return OK;
}

int melo_udp_shutdown(melo_udp_t * mudp)
{
    uv_udp_recv_stop(&mudp->uvudp);
    uv_close((uv_handle_t*) &mudp->uvudp, NULL);
    return OK;
}


int melo_udp_send_to_addr(melo_udp_t * mudp, const struct sockaddr* addr, const void* data, unsigned int datalen)
{
    uv_udp_send_t* req = (uv_udp_send_t*) malloc(sizeof(uv_udp_send_t) + datalen);
    uv_buf_t buf = uv_buf_init((char*)req + sizeof(uv_udp_send_t), datalen);
    memcpy(buf.base, data, datalen); // copy data to the end of req
    req->data = mudp;
    return (uv_udp_send(req, &mudp->uvudp, &buf, 1, addr, _cb_after_udp_send) == 0 ? 1 : 0);
}

int melo_udp_send_to_ip(melo_udp_t * mudp, const char* ip, int port, const void* data, unsigned int datalen)
{
    assert(ip);
    melo_sockaddr_t addr;

    melo_get_sock_addr(ip, port, &addr);
    return melo_udp_send_to_addr(mudp, (const struct sockaddr*) &addr, data, datalen);
}

int melo_udp_set_broadcast(melo_udp_t * mudp, int on)
{
    return (uv_udp_set_broadcast(&mudp->uvudp, on) == 0 ? 1 : 0);
}


static void _cb_on_udp_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned int flags)
{
    melo_udp_t * mudp = (melo_udp_t *) handle->data;

    // printf("on udp recv: size=%d \n", nread);
    if (nread > 0)
    {
        MELO_LISTENER(mudp->on_recv, mudp, buf->base, nread, addr, flags);
    }
    free(buf->base);
}

static void _cb_after_udp_send(uv_udp_send_t* req, int status)
{
    free(req); // see uvx_udp_send_to_addr()
}


