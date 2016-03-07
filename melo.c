#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include <uv.h>

#include "utils/automem.h"
#include "melo.h"

// Author: Liigo <com.liigo@gmail.com>.

static void _cb_after_send_to_stream(uv_write_t* w, int status);
static void _cb_after_send_mem(uv_write_t* w, int status);


void melo_get_sock_addr(const char* ip, int port, melo_sockaddr_t * sockaddr)
{
    MELO_ASSERT(NULL != ip, "ip pointer is null");
    MELO_ASSERT(NULL != sockaddr, "sockaddr pointer is null");

    if(strchr(ip, ':'))
    {
        uv_ip6_addr(ip, port, (struct sockaddr_in6*)&(sockaddr->addr6));
    }
    else
    {
        uv_ip4_addr(ip, port, (struct sockaddr_in*)&(sockaddr->addr4));
    }
}

const char* melo_get_ip_port(const struct sockaddr* addr, char* ipbuf, int buflen, int* port) 
{
    switch (addr->sa_family) 
    {
    case AF_INET: 
        {
            const struct sockaddr_in* addrin = (const struct sockaddr_in*) addr;
            if (ipbuf) { uv_inet_ntop(AF_INET, &(addrin->sin_addr), ipbuf, buflen); }
            if (port)  { *port = (int) ntohs(addrin->sin_port); }
            break;
        }
    case AF_INET6: 
        {
            const struct sockaddr_in6* addrin = (const struct sockaddr_in6*) addr;
            if (ipbuf) { uv_inet_ntop(AF_INET6, &(addrin->sin6_addr), ipbuf, buflen); }
            if (port)  { *port = (int) ntohs(addrin->sin6_port); }
            break;
        }
    default:
        if (port) { *port = 0; }
        return NULL;
    }
    return ipbuf ? ipbuf : NULL;
}

int melo_get_raw_ip_port(const struct sockaddr* addr, unsigned char* ipbuf, int* port) 
{
    switch (addr->sa_family) 
    {
    case AF_INET: 
        {
            const struct sockaddr_in* addrin = (const struct sockaddr_in*) addr;
            if(ipbuf) memcpy(ipbuf, &addrin->sin_addr, sizeof(addrin->sin_addr));
            if(port)  *port = (int) ntohs(addrin->sin_port);
            return sizeof(addrin->sin_addr); // 4
            break;
        }
    case AF_INET6: 
        {
            const struct sockaddr_in6* addrin = (const struct sockaddr_in6*) addr;
            if (ipbuf) memcpy(ipbuf, &addrin->sin6_addr, sizeof(addrin->sin6_addr));
            if (port)  *port = (int) ntohs(addrin->sin6_port);
            return sizeof(addrin->sin6_addr); // 16
            break;
        }
    default:
        if (port) *port = 0;
        return 0;
    }
}

const char * melo_get_tcp_ip_port(uv_tcp_t* uvclient, char* ipbuf, int buflen, int* port) 
{
    struct sockaddr addr;
    int len = sizeof(addr);
    int r = uv_tcp_getpeername(uvclient, &addr, &len);
    if (r == 0) 
    {
        return melo_get_ip_port(&addr, ipbuf, buflen, port);
    } 
    else 
    {
        printf("\n!!! [uvx] get client ip fails: %s\n", uv_strerror(r));
        return NULL;
    }
}



// Note: after this call, do not use `data` anymore, its memory will be `free`ed later.
int melo_send_to_stream(uv_stream_t * stream, void * data, unsigned int size) 
{
    // puts("uvx_send_to_stream()\n");
    assert(stream && data);
    uv_buf_t buf = { .base = (char*)data, .len = (size_t)size };
    uv_write_t * w = (uv_write_t*) malloc(sizeof(uv_write_t));
    memset(w, 0, sizeof(uv_write_t));
    w->data = data; // free it in uvx_after_send_to_stream()
    return (uv_write(w, stream, &buf, 1, _cb_after_send_to_stream) == 0 ? 1 : 0);
}

// Deprecated, use uvx_send_to_stream() instead.
// Note: after invoke uvx_send_mem(), do not use mem anymore, its memory will be freed later.
int melo_send_mem(uv_stream_t * stream, automem_t * mem) 
{
    // puts("uvx_send_mem()\n");
    assert(mem && mem->pdata);
    assert(stream);
    uv_buf_t buf = { .base = (char*)mem->pdata, .len = (size_t)mem->size };
    uv_write_t* w = (uv_write_t*) malloc(sizeof(uv_write_t));
    memset(w, 0, sizeof(uv_write_t));
    w->data = mem->pdata; // free it in after_send_mem()
    return uv_write(w, stream, &buf, 1, _cb_after_send_mem);
}

void melo_alloc_buf(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) 
{
    buf->base = (char*) malloc(suggested_size);
    buf->len  = buf->base ? suggested_size : 0;
}


//-----------------------------------------------------------------------------
// internal functions

static void _cb_after_send_mem(uv_write_t * w, int status) 
{
    MELO_ASSERT(NULL != w, "w is NULL");
    MELO_ASSERT(OK == status, "_cb_after_send_mem failed");

    //see uxv_send_mem()
    automem_t mem;
    mem.pdata = (unsigned char*)w->data;
    automem_uninit(&mem);

    free(w);
}

static void _cb_after_send_to_stream(uv_write_t * w, int status) 
{
    MELO_ASSERT(NULL != w, "w is NULL");
    MELO_ASSERT(OK == status, "_cb_after_send_to_stream failed");
    
    //see uvx_send_to_stream()
    //free(w->data);
    free(w);
}

#if defined(_WIN32) && !defined(__GNUC__)
#include <stdarg.h>
// Emulate snprintf() on Windows, _snprintf() doesn't zero-terminate the buffer on overflow...
int snprintf(char* buf, size_t len, const char* fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = _vsprintf_p(buf, len, fmt, ap);
    va_end(ap);

    /* It's a sad fact of life that no one ever checks the return value of
    * snprintf(). Zero-terminating the buffer hopefully reduces the risk
    * of gaping security holes.
    */
    if (n < 0)
    {
        if (len > 0)
        {
            buf[0] = '\0';
        }
    }
    return n;
}
#endif

