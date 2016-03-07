#ifndef __MELO_CLIENT_H
#define __MELO_CLIENT_H

#ifdef __cplusplus
extern "C"  {
#endif

#include "melo.h"

#define MELO_C_CBs \
    MELO_CB(UVX_C_ON_CONN_OK,      on_conn_ok,        void (*on_conn_ok)(melo_client_t *)) \
    MELO_CB(UVX_C_ON_CONN_FAIL,    on_conn_fail,      void (*on_conn_fail)(melo_client_t *)) \
    MELO_CB(UVX_C_ON_CONN_CLOSING, on_conn_closing,   void (*on_conn_closing)(melo_client_t *)) \
    MELO_CB(UVX_C_ON_CONN_CLOSE,   on_conn_close,     void (*on_conn_close)(melo_client_t *)) \
    MELO_CB(UVX_C_ON_SUPERVISOR,   on_supervisor,     void (*on_supervisor)(melo_client_t *, unsigned int)) \
    MELO_CB(UVX_C_ON_RECV,         on_recv,           void (*on_recv)(melo_client_t *, void *, ssize_t)) \


typedef struct melo_client_config_s
{
    char    name[32];               /* the xclient's name (with-ending-'\0') */
    char  * ip;                     /* ip address of server */
    int     port;                   /* port of server */
    bool_t  auto_connect;           /* 1: on, 0: off */
    int     supervisor_interval_ms;

    // logs
    FILE  * log_out;
    FILE  * log_err;
} melo_client_config_t;

typedef struct melo_client_s melo_client_t;

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) func_def;
typedef struct melo_client_s
{
    uv_loop_t * uvloop;
    uv_tcp_t    uvclient;
    uv_tcp_t  * uvserver; // &uvclient or NULL
    melo_client_config_t config;

    // callbacks
    MELO_C_CBs

    uv_connect_t        conn;                  // sizeof(uv_connect_t) == 64
    melo_sockaddr_t     server_addr;            // sizeof(uvx_sockaddr_4_6_t) == 28
    uv_timer_t          supervisor_timer;         // sizeof(uv_timer_t) == 120
    unsigned int        supervisor_index;
    bool_t              connection_closed; /* indicate connection closed or not */

    void* data;
}melo_client_t;

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) id,
typedef enum
{
    MELO_C_CBs
    MELO_C_ON_MAX,
}melo_client_listener_t;

void melo_client_init(melo_client_t * mclient);

void melo_client_reg_listener(melo_client_t * mclient, melo_client_listener_t type, void * listener);

// returns the default config for xclient, used by uvx_client_connect().
melo_client_config_t melo_client_default_config(melo_client_t * mclient);

// connect to an tcp server (not only xserver) that listening ip:port. support IPv4 and IPv6.
// please pass in uninitialized xclient and initialized config.
// returns 1 on success, or 0 if fails.
int melo_client_connect(melo_client_t * mclient, uv_loop_t* loop, melo_client_config_t config);

// disconnect the current connection (and it will re-connect at next heartbeat timer).
// returns 1 on success, or 0 if fails.
int melo_client_disconnect(melo_client_t * mclient);

// shutdown the xclient normally.
// returns 1 on success, or 0 if fails.
int melo_client_shutdown(melo_client_t * mclient);

// send data to the connected tcp server (not only xserver).
// don't use `data` any more, it will be `free`ed later.
// please make sure that `data` was `malloc`ed before, so that it can be `free`ed correctly.
// returns 1 on success, or 0 if fails.
int melo_client_send(melo_client_t * mclient, void* data, unsigned int size);


#ifdef __cplusplus
extern "C" {
#endif

#endif /* __MELO_CLIENT_H */

