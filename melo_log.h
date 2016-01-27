#ifndef __MELO_LOG_H
#define __MELO_LOG_H

#ifdef __cplusplus
extern "C"  {
#endif

#include "melo.h"
#include "melo_udp.h"

typedef struct melo_log_s
{
    uv_loop_t     * uvloop;
    melo_udp_t      mudp;
    melo_sockaddr_t target_addr;
    loge_t          loge;
} melo_log_t;

// predefined log levels
#define MELO_LOG_ALL    LOGE_LOG_ALL
#define MELO_LOG_TRACE  LOGE_LOG_TRACE
#define MELO_LOG_DEBUG  LOGE_LOG_DEBUG
#define MELO_LOG_INFO   LOGE_LOG_INFO
#define MELO_LOG_WARN   LOGE_LOG_WARN
#define MELO_LOG_ERROR  LOGE_LOG_ERROR
#define MELO_LOG_FATAL  LOGE_LOG_FATAL
#define MELO_LOG_NONE   LOGE_LOG_NONE

// init an uvx log with uv loop, ip, port and name.
// please pass in uninitialized xlog. name can be NULL, default to "xlog".
// logs are sent to its target (target_ip:target_port) through IPv4/IPv6 + UDP.
// returns 1 on success, or 0 if fails.
int melo_log_init(melo_log_t * mlog, uv_loop_t* loop, const char* target_ip, int target_port, const char* name);

// send a log to target through UDP.
// level: see UVX_LOG_*; tags: comma separated text; msg: log content text.
// file and line: the source file path+name and line number.
// parameter tags/msg/file can be NULL, and may be truncated if too long.
// all text parameters should be utf-8 encoded, or utf-8 compatible.
// returns 1 on success, or 0 if fails.
// note: be limited by libuv, we should call `uvx_log_send` only in its main loop thread.
int melo_log_send(melo_log_t * mlog, int level, const char* tags, const char* msg, const char* file, int line);

// the binary version of `uvx_log_send`, `msg` is a binary data instead of a text.
// see `uvx_log_send` for more details.
int melo_log_send_bin(melo_log_t * mlog, int level, const char* tags,
                     const void* msg, unsigned int msglen, const char* file, int line);

// serialize a log into bytes stream, ready to be sent by `uvx_log_send_serialized` later.
// buf and bufsize: an output buffer and its size, recommend 1024, can be smaller or larger.
// the other parameters are as same as `uvx_log_send`.
// returns the serialized data size in bytes, which is ensure not exceed bufsize and 1024.
// maybe returns 0, which means nothing was serialized (e.g. when the log was disabled).
// example:
//   char buf[1024];
//   unsigned int len = uvx_log_serialize(xlog, buf, sizeof(buf), ...);
//   uvx_log_send_serialized(xlog, buf, len);
unsigned int
melo_log_serialize(melo_log_t * mlog, void* buf, unsigned int bufsize,
                  int level, const char* tags, const char* msg, const char* file, int line);

// the binary version of `uvx_log_serialize`, `msg` is a binary data instead of a text.
// returns 0 if the msg's size is too long.
// see `uvx_log_serialize` for more details.
unsigned int
melo_log_serialize_bin(melo_log_t * mlog, void* buf, unsigned int bufsize, int level, const char* tags,
                      const void* msg, unsigned int msglen, const char* file, int line);

// send a serialized log to target through UDP.
// the parameter `data`/`datalen` must be serialized by `uvx_log_serialize[_bin]` before.
// returns 1 on success, or 0 if fails.
// note: be limited by libuv, we should call `uvx_log_send_serialized` only in its main loop thread.
int melo_log_send_serialized(melo_log_t * mlog, const void* data, unsigned int datalen);

// to enable (if enabled==1) or disable (if enabled==0) the log
void melo_log_enable(melo_log_t * mlog, int enabled);


// a printf-like MELO_LOG utility macro, to format and send a log.
// parameters:
//   log: an melo_log_t* which already uvx_log_start()-ed
//   level: one of MELO_LOG_* consts
//   tags: comma separated text
//   msgfmt: the format text for msg, e.g. "something %s %d or else"
//   ...: the values that match %* in msgfmt
// examples:
//   MELO_LOG(&log, UVX_LOG_INFO, "uvx,liigo", "%d %s", 123, "liigo");
//   MELO_LOG(&log, UVX_LOG_INFO, "uvx,liigo", "pure text without format", NULL);
#define MELO_LOG(mlog,level,tags,msgfmt,...) \
{\
    char melo_tmp_msg_[LOGE_MAXBUF]; /* avoid name conflict with outer-scope names */ \
    snprintf(melo_tmp_msg_, sizeof(melo_tmp_msg_), msgfmt, __VA_ARGS__);\
    melo_log_send(mlog, level, tags, melo_tmp_msg_, __FILE__, __LINE__);\
}

// only serialize a log, but not send it.
// `bufsize` will be rewrite to fill in the serialized size.
// example:
//   char buf[1024]; unsigned int len = sizeof(buf);
//   MELO_LOG_SERIALIZE(&xlog, buf, len, UVX_LOG_INFO, "author", "name: %s, sex: %d", "Liigo", 1);
//   melo_log_send_serialized(&xlog, buf, len);
#define MELO_LOG_SERIALIZE(mlog,buf,bufsize,level,tags,msgfmt,...) \
{\
    char melo_tmp_msg_[LOGE_MAXBUF]; /* avoid name conflict with outer-scope names */ \
    snprintf(melo_tmp_msg_, sizeof(melo_tmp_msg_), msgfmt, __VA_ARGS__);\
    bufsize = melo_log_serialize(mlog, buf, bufsize, level, tags, melo_tmp_msg_, __FILE__, __LINE__);\
}


#ifdef __cplusplus
}
#endif

#endif /* __MELO_LOG_H */

