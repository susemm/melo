#ifndef __MELO_H
#define __MELO_H

#ifdef __cplusplus
extern "C"	{
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <uv.h>
#include "loge/loge.h"
#include "utils/automem.h"

//-----------------------------------------------
// uvx: a lightweight wrapper of libuv, defines `uvx_server_t`(TCP server),
// `uvx_client_t`(TCP client) and `uvx_udp_t`(UDP), along with an `uvx_log_t`(UDP logger).
//
// - To define a TCP server (which I call it xserver), you're just required to provide
//   a `uvx_server_config_t` with some params and callbacks, then call `uvx_server_start`.
// - To define a TCP client (which I call it xclient), you're just required to provide
//   a `uvx_client_config_t` with some params and callbacks, then call `uvx_client_connect`.
// - To define an UDP service (which I call it xudp), you're just required to provide
//   a `uvx_udp_config_t` with some params and callbacks, then call `uvx_udp_start`.
// - To define a logging service (which I call it xlog), just call `uvx_log_init`.
//
// There are a few predefined callbacks, such as on_conn_ok, on_conn_close, on_heartbeat, etc.
// All callbacks are optional. Maybe `on_recv` is the most useful one you care about.
//
// by Liigo 2014-11.
// http://github.com/liigo/uvx


//-----------------------------------------------

#if defined(_MSC_VER)
    #define MELO_INLINE /*__inline*/
#else
    #define MELO_INLINE inline
#endif


#define OK      (0)
#define ERROR   (-1)
#define TRUE    (1)
#define FALSE   (0)

typedef unsigned char uint8_t;
typedef uint8_t bool_t;

#define MELO_ASSERT(expr, msg)            if (expr) {} else { printf(msg); return; }
#define MELO_ASSERT_RET(expr, msg, ret)   if (expr) {} else { printf(msg); return (ret); }

#define MELO_LISTENER(func, ...) if ((func) != NULL) { (func)(__VA_ARGS__); } else {printf("%s not reg\n", #func);}

#define MELO_INFO(fp, ...) if (NULL != (fp)) { fprintf((fp), __VA_ARGS__); }
#define MELO_ERR(fp, ...)  if (NULL != (fp)) { fprintf((fp), __VA_ARGS__); }


typedef union melo_sockaddr_s
{
    struct sockaddr     addr;
    struct sockaddr_in  addr4;
    struct sockaddr_in6 addr6;
} melo_sockaddr_t;

void melo_get_sock_addr(const char* ip, int port, melo_sockaddr_t * sockaddr);

// get text presentation ip address, writing to ipbuf and returns ipbuf. writing port if not NULL.
// support IPv4 (buflen = 16) and IPv6 (buflen = 40).
const char* melo_get_ip_port(const struct sockaddr * addr, char * ipbuf, int buflen, int* port);

// get binary presentation ip address, writing to ipbuf and returns lengh in bytes of it. writing port if not NULL.
// support IPv4 (buflen = 4) and IPv6 (buflen = 16).
int melo_get_raw_ip_port(const struct sockaddr * addr, unsigned char * ipbuf, int * port);

// get the tcp-client's text presentation ip address, writing to ipbuf and returns ipbuf. writing port if not NULL.
// support IPv4 (buflen = 16) and IPv6 (buflen = 40).
const char* melo_get_tcp_ip_port(uv_tcp_t * uvclient, char * ipbuf, int buflen, int * port);

// send data to a libuv stream. don't use `data` any more, it will be `free`ed later.
// please make sure that `data` was `malloc`ed before, so that it can be `free`ed correctly.
// returns 1 on success, or 0 if fails.
int melo_send_to_stream(uv_stream_t * stream, void * data, unsigned int size);

// deprecated, use uvx_send_to_stream() instead.
int melo_send_mem(automem_t* mem, uv_stream_t * stream);

void melo_alloc_buf(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf);


#if defined(_WIN32) && !defined(__GNUC__)
#include <stdarg.h>
// Emulate snprintf() on Windows, _snprintf() doesn't zero-terminate the buffer on overflow...
int snprintf(char * buf, size_t len, const char* fmt, ...);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __MELO_H */

