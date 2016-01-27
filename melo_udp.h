#ifndef __MELO_UDP_H
#define __MELO_UDP_H

#ifdef __cplusplus
extern "C"  {
#endif

#include "melo.h"

#define MELO_U_CBs \
    MELO_CB(MELO_UDP_ON_RECV, on_recv, void (*on_recv)(melo_udp_t *, void *, ssize_t, const struct sockaddr* addr, unsigned int flags)) \

typedef struct melo_udp_config_s
{
    char name[32];    // the xudp's name (with-ending-'\0')
    char * ip;
    int port;

    // logs
    FILE * log_out;
    FILE * log_err;
} melo_udp_config_t;

typedef struct melo_udp_s melo_udp_t;

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) func_def;
typedef struct melo_udp_s
{
    uv_loop_t       * uvloop;
    uv_udp_t          uvudp;
    melo_udp_config_t  config;

    // callbacks
    MELO_U_CBs

    void            * data;
}melo_udp_t;

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) id,
typedef enum
{
    MELO_U_CBs
    MELO_U_ON_MAX,
}melo_udp_listener_t;


void melo_udp_init(melo_udp_t * mudp);

void melo_udp_reg_listener(melo_udp_t * mudp, melo_udp_listener_t type, void * listener);

// returns the default config for xudp, used by uvx_udp_start().
melo_udp_config_t melo_udp_default_config(melo_udp_t * mudp);

// start the udp service bind on ip:port. support IPv4 and IPv6.
// if ip == NULL, bind to local random port automatically.
// please pass in uninitialized xudp and initialized config.
// returns 1 on success, or 0 if fails.
int melo_udp_start(melo_udp_t * mudp, uv_loop_t* loop, melo_udp_config_t config);

// shutdown the xudp normally.
// returns 1 on success, or 0 if fails.
int melo_udp_shutdown(melo_udp_t * mudp);

// send data through udp. support IPv4 and IPv6.
// will copy data internally, so caller can take it easy to free it after this call.
int melo_udp_send_to_ip(melo_udp_t * mudp, const char* ip, int port, const void* data, unsigned int datalen);
int melo_udp_send_to_addr(melo_udp_t * mudp, const struct sockaddr* addr, const void* data, unsigned int datalen);

// set broadcast on (1) or off (0)
// returns 1 on success, or 0 if fails.
int melo_udp_set_broadcast(melo_udp_t * mudp, int on);


#ifdef __cplusplus
}
#endif

#endif /* __MELO_UDP_H */

