#ifndef __MELO_SERVER_H
#define __MELO_SERVER_H

#ifdef __cplusplus
extern "C"  {
#endif

#include "melo.h"

#define MELO_S_CBs \
    MELO_CB(UVX_S_ON_CONN_OK,      on_conn_ok,        void (*on_conn_ok)(melo_server_t *, melo_server_conn_t *)) \
    MELO_CB(UVX_S_ON_CONN_FAIL,    on_conn_fail,      void (*on_conn_fail)(melo_server_t *, melo_server_conn_t *)) \
    MELO_CB(UVX_S_ON_CONN_CLOSING, on_conn_closing,   void (*on_conn_closing)(melo_server_t *, melo_server_conn_t *)) \
    MELO_CB(UVX_S_ON_CONN_CLOSE,   on_conn_close,     void (*on_conn_close)(melo_server_t *, melo_server_conn_t *)) \
    MELO_CB(UVX_S_ON_SUPERVISOR,   on_supervisor,     void (*on_supervisor)(melo_server_t *, unsigned int)) \
    MELO_CB(UVX_S_ON_RECV,         on_recv,           void (*on_recv)(melo_server_t *, melo_server_conn_t *, void *, ssize_t)) \


typedef struct melo_server_config_s
{
    char name[32];    // the xserver's name (with-ending-'\0')
    char * ip;
    int port;
    int conn_count;     // estimated connections count
    int conn_backlog;   // used by uv_listen()
    int conn_extra_size; // the bytes of extra data, see `uvx_server_conn_t.extra`
    int conn_timeout_ms; // if > 0, timeout-ed connections will be closed
    int supervisor_interval_ms; // used by heartbeat timer

    // logs
    FILE    * log_out;
    FILE    * log_err;
} melo_server_config_t;

typedef struct melo_server_s melo_server_t;
typedef struct melo_server_conn_s melo_server_conn_t;

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) func_def;
typedef struct melo_server_s
{
    uv_loop_t             * uvloop;
    uv_tcp_t                uvserver;
    melo_server_config_t    config;

    MELO_S_CBs

    melo_sockaddr_t     server_addr;
    uv_timer_t          supervisor_timer; // sizeof(uv_timer_t) == 120
    unsigned int        supervisor_index;
    struct lh_table   * conns;

    //unsigned char privates[136]; // to store uvx_server_private_t
    void              * data; // for public use
}melo_server_t;

typedef struct melo_server_conn_s
{
    melo_server_t * mserver;
    uv_tcp_t        uvclient;
    uint64_t        last_comm_time; // time of last communication (uv_now(loop))
    int             refcount;
    uv_mutex_t      refmutex;
    void          * extra;          // pointer to extra data, if config.conn_extra_size > 0, or else is NULL
    // extra data resides here
} melo_server_conn_t;

typedef void (*UVX_S_ON_ITER_CONN) (melo_server_t * mserver, melo_server_conn_t* conn, void* userdata);

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) id,
typedef enum
{
    MELO_S_CBs
    UVX_S_ON_MAX,
}melo_server_listener_t;


void melo_server_init(melo_server_t * mserver);

void melo_server_reg_listener(melo_server_t * mserver, melo_server_listener_t type, void * listener);

// returns the default config for xserver, used by uvx_server_start().
melo_server_config_t melo_server_default_config(melo_server_t * mserver);

// start an xserver listening on ip:port, support IPv4 and IPv6.
// please pass in uninitialized xserver and initialized config.
// returns 1 on success, or 0 if fails.
int melo_server_start(melo_server_t * mserver, uv_loop_t* loop, melo_server_config_t config);

// shutdown the xserver normally.
// returns 1 on success, or 0 if fails.
int melo_server_shutdown(melo_server_t * mserver);

// iterate all connections if on_iter_conn != NULL.
// returns the number of connections.
int melo_server_iter_conns(melo_server_t * mserver, UVX_S_ON_ITER_CONN on_iter_conn, void* userdata);

// manager conn refcount manually, +1 or -1, free conn when refcount == 0. threadsafe.
void melo_server_conn_ref(melo_server_conn_t * conn, int ref);

// send data to tcp client (not only xclient) of the connection.
// don't use `data` any more, it will be `free`ed later.
// please make sure that `data` was `malloc`ed before, so that it can be `free`ed correctly.
// returns 1 on success, or 0 if fails.
int melo_server_conn_send(melo_server_conn_t * conn, void* data, unsigned int size);


#ifdef __cplusplus
extern "C"  {
#endif

#endif /* __MELO_SERVER_H */

