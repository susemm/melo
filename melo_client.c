#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>

#include "melo_client.h"
#include "utils/automem.h"
#include "utils/linkhash.h"

// Author: Liigo <com.liigo@gmail.com>.


static int _client_reconnect(melo_client_t * mclient);
static void _client_close(melo_client_t* mclient);
static void _cb_on_supervisor_timer(uv_timer_t* handle);
static void _cb_after_close_client(uv_handle_t* handle);
static void _cb_on_client_read(uv_stream_t* uvserver, ssize_t nread, const uv_buf_t* buf);
static void _cb_on_connect(uv_connect_t* conn, int status);


void melo_client_init(melo_client_t * mclient)
{
    MELO_ASSERT(mclient != NULL, "mclient is NULL");
    memset(mclient, 0, sizeof(melo_client_t));
}

void melo_client_reg_listener(melo_client_t * mclient, melo_client_listener_t type, void * listener)
{
    MELO_ASSERT(mclient != NULL, "mclient is NULL");
    MELO_ASSERT(type < MELO_C_ON_MAX, "invalid type");

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) case id: mclient->func = listener; break;

    switch (type)
    {
        MELO_C_CBs

        default:
            break;
    }
}


melo_client_config_t melo_client_default_config(melo_client_t* mclient)
{
    melo_client_config_t config = { 0 };
    snprintf(config.name, sizeof(config.name), "mclient-%p", mclient);
    config.auto_connect = TRUE;
    config.supervisor_interval_ms = 60 * 1000;
    config.log_out = stdout;
    config.log_err = stderr;
    return config;
}


int melo_client_connect(melo_client_t * mclient, uv_loop_t * loop, melo_client_config_t config)
{
    MELO_ASSERT_RET(NULL != mclient, "mclient is null", 0);
    MELO_ASSERT_RET(NULL != loop, "loop is null", 0);

    mclient->uvloop = loop;
    mclient->uvserver = NULL;
    mclient->connection_closed = FALSE;

    memcpy(&mclient->config, &config, sizeof(melo_client_config_t));

    melo_get_sock_addr(mclient->config.ip, mclient->config.port, &(mclient->server_addr));

    int ret = _client_reconnect(mclient);

    int timeout = config.supervisor_interval_ms; // in milliseconds
    mclient->supervisor_index = 0;
    mclient->supervisor_timer.data = mclient;
    uv_timer_init(loop, &(mclient->supervisor_timer));
    uv_timer_start(&(mclient->supervisor_timer), _cb_on_supervisor_timer, timeout, timeout);

    return ret;
}

int melo_client_disconnect(melo_client_t * mclient)
{
    MELO_ASSERT_RET(NULL != mclient, "mclient is null", ERROR);
    _client_close(mclient);
    return 1;
}

int melo_client_shutdown(melo_client_t * mclient)
{
    MELO_ASSERT_RET(NULL != mclient, "mclient is null", ERROR);
    uv_timer_stop(&(mclient->supervisor_timer));
    _client_close(mclient);
    return 1;
}

int melo_client_send(melo_client_t * mclient, void * data, unsigned int size)
{
    MELO_ASSERT_RET(NULL != mclient, "mclient is null", 0);
    MELO_ASSERT_RET(NULL != mclient->uvserver, "mclient->uvserver is null", 0);

    return melo_send_to_stream((uv_stream_t*)mclient->uvserver, data, size);
}

static int _client_reconnect(melo_client_t * mclient)
{
    MELO_ASSERT_RET(NULL != mclient, "mclient is null", ERROR);

    mclient->uvserver = NULL;
    mclient->connection_closed = FALSE;
    mclient->conn.data = mclient;
    uv_tcp_init(mclient->uvloop, &mclient->uvclient);
    int ret = uv_tcp_connect(&(mclient->conn), &mclient->uvclient,
                             (const struct sockaddr *)&(mclient->server_addr), _cb_on_connect);
    if(ret >= 0)
    {
        MELO_INFO(mclient->config.log_out,
                "[melo-client] %s connect to server ...\n",
                mclient->config.name);
    }
    else
    {
        MELO_ERR(mclient->config.log_err,
                "\n!!! [melo-client] %s connect failed: %s\n",
                mclient->config.name, uv_strerror(ret));
    }
    return (ret >= 0 ? 1 : 0);
}

static void _client_close(melo_client_t * mclient)
{
    MELO_ASSERT(NULL != mclient, "mclient is null");

    MELO_INFO(mclient->config.log_out,
            "[melo-client] %s on close\n",
            mclient->config.name);

    MELO_LISTENER(mclient->on_conn_closing, mclient);

    uv_close((uv_handle_t*) &mclient->uvclient, _cb_after_close_client);

    // heartbeat_timer is reused to re-connect on next uvx__on_heartbeat_timer(), do not stop it.
    // uv_timer_stop(&_context.heartbeat_timer);
    // uv_close((uv_handle_t*)&_context.heartbeat_timer, NULL);
}

static void _cb_on_supervisor_timer(uv_timer_t * handle)
{
    melo_client_t * mclient = (melo_client_t *) handle->data;
    MELO_ASSERT(NULL != mclient, "mclient is null");

    if (mclient->uvserver)
    {
        unsigned int index = mclient->supervisor_index++;
        MELO_INFO(mclient->config.log_out,
                "[melo-client] %s on supervisor (index %u)\n",
                mclient->config.name, index);
        MELO_LISTENER(mclient->on_supervisor, mclient, index)
    }
    else
    {
        if (mclient->connection_closed)
        {
            _client_reconnect(mclient); // auto reconnect
        }
    }
}

static void _cb_after_close_client(uv_handle_t * handle)
{
    melo_client_t * mclient = (melo_client_t *) handle->data;

    assert(handle->data);

    MELO_LISTENER(mclient->on_conn_close, mclient);
    mclient->uvserver = NULL;
    mclient->connection_closed = TRUE;
}

static void _cb_on_client_read(uv_stream_t * uvserver, ssize_t nread, const uv_buf_t * buf)
{
    melo_client_t * mclient = (melo_client_t *) uvserver->data;
    assert(mclient);

    if(nread > 0)
    {
        assert(mclient->uvserver == (uv_tcp_t*)uvserver);
        MELO_LISTENER(mclient->on_recv, mclient, buf->base, nread)
    }
    else if(nread < 0)
    {
        uv_read_stop(uvserver);

        MELO_ERR(mclient->config.log_err,
                "\n!!! [melo-client] %s on recv error: %s\n",
                mclient->config.name, uv_strerror(nread));

        _client_close(mclient); // will try reconnect on next uvx__on_heartbeat_timer()
    }
    free(buf->base);
}


static void _cb_on_connect(uv_connect_t* conn, int status)
{
    melo_client_t * mclient = (melo_client_t *) conn->data;
    mclient->uvclient.data = mclient;

    if(OK == status)
    {
        MELO_INFO(mclient->config.log_out,
                "[melo-client] %s connect to server ok\n", mclient->config.name);

        assert(conn->handle == (uv_stream_t*) &mclient->uvclient);
        mclient->uvserver = (uv_tcp_t*) conn->handle;

        MELO_LISTENER(mclient->on_conn_ok, mclient);

        uv_read_start(conn->handle, melo_alloc_buf, _cb_on_client_read);
    }
    else
    {
        mclient->uvserver = NULL;

        MELO_ERR(mclient->config.log_err,
                "\n!!! [melo-client] %s connect to server failed: %s\n",
                mclient->config.name, uv_strerror(status));

        MELO_LISTENER(mclient->on_conn_fail, mclient);

        _client_close(mclient); // will try reconnect on next on_heartbeat
    }
}


