#include <math.h>
#include <assert.h>
#include "melo_log.h"

// Author: Liigo <com.liigo@gmail.com>, 201407.

int melo_log_init(melo_log_t * mlog, uv_loop_t* loop, const char* target_ip, int target_port, const char* name)
{
    mlog->uvloop = loop;
    assert(target_ip);

    melo_get_sock_addr(target_ip, target_port, &mlog->target_addr);

    loge_init(&mlog->loge, name);

    melo_udp_init(&mlog->mudp);
    melo_udp_config_t config = melo_udp_default_config(&mlog->mudp);
    config.ip = NULL;
    config.port = 0;
    return melo_udp_start(&mlog->mudp, loop, config);
}

MELO_INLINE
void melo_log_enable(melo_log_t * mlog, int enabled)
{
    loge_enable(&mlog->loge, enabled);
}

MELO_INLINE unsigned int
melo_log_serialize(melo_log_t * mlog, void* buf, unsigned int bufsize,
                  int level, const char* tags, const char* msg, const char* file, int line)
{
    return loge_item(&mlog->loge, buf, bufsize, level, tags, msg, file, line);
}

MELO_INLINE unsigned int
melo_log_serialize_bin(melo_log_t * mlog, void* buf, unsigned int bufsize, int level, const char* tags,
                      const void * msg, unsigned int msglen, const char* file, int line)
{
    return loge_item_bin(&mlog->loge, buf, bufsize, level, tags, msg, msglen, file, line);
}

MELO_INLINE
int melo_log_send(melo_log_t * mlog, int level, const char* tags, const char* msg, const char* file, int line)
{
    char buf[LOGE_MAXBUF];
    unsigned int size = loge_item(&mlog->loge, buf, sizeof(buf), level, tags, msg, file, line);
    // send out through udp
    return melo_udp_send_to_addr(&mlog->mudp, &mlog->target_addr.addr, buf, size);
}

MELO_INLINE
int melo_log_send_bin(melo_log_t * mlog, int level, const char* tags,
                     const void* msg, unsigned int msglen, const char* file, int line)
{
    char buf[LOGE_MAXBUF];
    unsigned int size = loge_item_bin(&mlog->loge, buf, sizeof(buf), level, tags, msg, msglen, file, line);
    // send out through udp
    return melo_udp_send_to_addr(&mlog->mudp, &mlog->target_addr.addr, buf, size);
}

MELO_INLINE
int melo_log_send_serialized(melo_log_t* mlog, const void* data, unsigned int datalen)
{
    return melo_udp_send_to_addr(&mlog->mudp, &mlog->target_addr.addr, data, datalen);
}
