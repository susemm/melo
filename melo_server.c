#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

#include "melo_server.h"
#include "utils/automem.h"
#include "utils/linkhash.h"

// Author: Liigo <com.liigo@gmail.com>.

static void _cb_on_read(uv_stream_t* uvclient, ssize_t nread, const uv_buf_t* buf);
static void _cb_on_connection(uv_stream_t* uvserver, int status);
static void _cb_after_close_connection(uv_handle_t* handle);
static void _cb_on_supervisor_timer(uv_timer_t* handle);
static void _disconnect_client(uv_stream_t* uvclient);
static void _check_timeout_clients(melo_server_t * mserver);


void melo_server_init(melo_server_t * mserver)
{
    MELO_ASSERT(mserver != NULL, "mserver is NULL");
    memset(mserver, 0, sizeof(melo_server_t));
}

void melo_server_reg_listener(melo_server_t * mserver, melo_server_listener_t type, void * listener)
{
    MELO_ASSERT(mserver != NULL, "mserver is NULL");
    MELO_ASSERT(type < UVX_S_ON_MAX, "invalid type");

#ifdef MELO_CB
#undef MELO_CB
#endif
#define MELO_CB(id, func, func_def) case id: mserver->func = listener; break;

    switch (type)
    {
        MELO_S_CBs

        default:
            break;
    }
}

melo_server_config_t melo_server_default_config(melo_server_t * mserver)
{
    melo_server_config_t config = { 0 };

    snprintf(config.name, sizeof(config.name), "mserver-%p", mserver);
    config.conn_count = 1024;
    config.conn_backlog = 128;
    config.conn_extra_size = 0;
    config.conn_timeout_ms = 180000;
    config.supervisor_interval_ms = 60000;
    config.log_out = stdout;
    config.log_err = stderr;

    return config;
}

int melo_server_start(melo_server_t * mserver, uv_loop_t* loop, melo_server_config_t config)
{
    assert(mserver && loop);
    mserver->uvloop = loop;
    memcpy(&mserver->config, &config, sizeof(melo_server_config_t));

    mserver->conns = lh_kptr_table_new(config.conn_count, "clients connection table", NULL);

    // init and start timer
    uv_timer_init(loop, &(mserver->supervisor_timer));
    mserver->supervisor_timer.data = mserver;
    mserver->supervisor_index = 0;
    int timeout = config.supervisor_interval_ms; // in milliseconds
    if(timeout > 0)
    {
        uv_timer_start(&(mserver->supervisor_timer), _cb_on_supervisor_timer, timeout, timeout);
    }

    // init tcp, bind and listen
    uv_tcp_init(loop, &mserver->uvserver);
    mserver->uvserver.data = mserver;

    melo_get_sock_addr(mserver->config.ip, mserver->config.port, &(mserver->server_addr));
    uv_tcp_bind(&mserver->uvserver, (const struct sockaddr*) &(mserver->server_addr), 0);

    int ret = uv_listen((uv_stream_t*)&mserver->uvserver, config.conn_backlog, _cb_on_connection);
    if(ret >= 0)
    {
        char timestr[32]; time_t t; time(&t);
        strftime(timestr, sizeof(timestr), "[%Y-%m-%d %X]", localtime(&t)); // C99 only: %F = %Y-%m-%d
        MELO_INFO(config.log_out,
                "[melo-server] %s %s listening on %s:%d ...\n",
                timestr, mserver->config.name, mserver->config.ip, mserver->config.port);
    }
    else
    {
        MELO_ERR(config.log_err,
                "\n!!! [melo-server] %s listen on %s:%d failed: %s\n",
                mserver->config.name, mserver->config.ip, mserver->config.port, uv_strerror(ret));
    }

    return (ret >= 0);
}

int melo_server_shutdown(melo_server_t * mserver)
{
    MELO_ASSERT_RET(NULL != mserver, "xserver is null", ERROR);

    uv_timer_stop(&(mserver->supervisor_timer));
    uv_close((uv_handle_t*)&(mserver->supervisor_timer), NULL);
    lh_table_free(mserver->conns);
    uv_close((uv_handle_t*)&mserver->uvserver, NULL);

    return 0;
}

void melo_server_conn_ref(melo_server_conn_t * conn, int ref)
{
    MELO_ASSERT(NULL != conn, "conn is null");
    MELO_ASSERT(ref == 1 || ref == -1, "invalid ref");

    uv_mutex_lock(&conn->refmutex);
    conn->refcount += ref; // +1 or -1
    if(conn->refcount == 0)
    {
        uv_mutex_unlock(&conn->refmutex);
        uv_mutex_destroy(&conn->refmutex);
        free(conn);
        return;
    }
    uv_mutex_unlock(&conn->refmutex);
}

int melo_server_conn_send(melo_server_conn_t * conn, void* data, unsigned int size)
{
    MELO_ASSERT_RET(NULL != conn, "conn is null", ERROR);
    return melo_send_to_stream((uv_stream_t*)&conn->uvclient, data, size);
}

int melo_server_iter_conns(melo_server_t * mserver, UVX_S_ON_ITER_CONN on_iter_conn, void* userdata)
{
    MELO_ASSERT_RET(NULL != on_iter_conn, "on_iter_conn pointer is null", mserver->conns->count) ;

    struct lh_entry *e, *tmp;
    lh_foreach_safe(mserver->conns, e, tmp)
    {
        on_iter_conn(mserver, (melo_server_conn_t *) e->k, userdata);
    }

    return mserver->conns->count;
}


static void _cb_on_read(uv_stream_t* uvclient, ssize_t nread, const uv_buf_t* buf)
{
    melo_server_conn_t * conn = (melo_server_conn_t *) uvclient->data;
    assert(conn);
    melo_server_t * mserver = conn->mserver;
    assert(mserver);
    if(nread > 0)
    {
        /**
         * update the time of latest communicating, delete then insert depend on lh_table, to ensure the latest is at tail.
         * see also _uvx_check_timeout_clients()
         */
        conn->last_comm_time = uv_now(mserver->uvloop);
        int n = lh_table_delete(mserver->conns, (const void*)conn);
        assert(n == 0); //delete success
        lh_table_insert(mserver->conns, conn, (const void*)conn);

        MELO_LISTENER(mserver->on_recv, mserver, conn, buf->base, nread)
    }
    else if(nread < 0)
    {
        MELO_ERR(mserver->config.log_err,
                "\n!!! [melo-server] %s on recv error: %s\n",
                mserver->config.name, uv_strerror(nread));

        _disconnect_client(uvclient);
    }
    free(buf->base);
}

static void _cb_on_connection(uv_stream_t* uvserver, int status)
{
    melo_server_t * mserver = (melo_server_t *) uvserver->data;
    assert(mserver);
    if(status == 0)
    {
        MELO_INFO(mserver->config.log_out, "[melo-server] %s on connection\n", mserver->config.name);

        assert(uvserver == (uv_stream_t*) &mserver->uvserver);
        assert(mserver->config.conn_extra_size >= 0);

        // Create new connection
        melo_server_conn_t * conn = (melo_server_conn_t *) calloc(1, sizeof(melo_server_conn_t) + mserver->config.conn_extra_size);
        if (mserver->config.conn_extra_size > 0)
        {
            conn->extra = (void*)(conn + 1);
        }
        conn->mserver = mserver;
        conn->uvclient.data = conn;
        conn->last_comm_time = 0;
        conn->refcount = 1;
        uv_mutex_init(&conn->refmutex);

        // Save to connection list
        assert(lh_table_lookup_entry(mserver->conns, conn) == NULL);
        lh_table_insert(mserver->conns, conn, (const void*)conn);

        uv_tcp_init(mserver->uvloop, &conn->uvclient);
        if(uv_accept(uvserver, (uv_stream_t*) &conn->uvclient) == 0)
        {
            conn->last_comm_time = uv_now(mserver->uvloop);
            MELO_LISTENER(mserver->on_conn_ok, mserver, conn);
            uv_read_start((uv_stream_t*) &conn->uvclient, melo_alloc_buf, _cb_on_read);
        }
        else
        {
            MELO_LISTENER(mserver->on_conn_fail, conn->mserver, conn);
            uv_close((uv_handle_t*) &conn->uvclient, _cb_after_close_connection);
        }
    }
    else
    {
        MELO_INFO(mserver->config.log_err,
                "\n!!! [melo-server] %s on connection error: %s\n",
                mserver->config.name, uv_strerror(status));
    }
}

static void _cb_after_close_connection(uv_handle_t* handle)
{
    melo_server_conn_t * conn = (melo_server_conn_t *) handle->data;
    assert(conn && conn->mserver);
    melo_server_t * mserver = conn->mserver;

    MELO_LISTENER(mserver->on_conn_close, mserver, conn);

    int n = lh_table_delete(mserver->conns, (const void*)conn);
    assert(n == 0); //delete success
    melo_server_conn_ref(conn, -1); // call on_conn_close() inside here? in non-main-thread?
}


static void _cb_on_supervisor_timer(uv_timer_t* handle)
{
    melo_server_t * mserver = (melo_server_t *) handle->data;
    assert(mserver);
    unsigned int index = mserver->supervisor_index++;

    MELO_INFO(mserver->config.log_out,
            "[melo-server] %s on supervisor (index %u)\n",
            mserver->config.name,
            index);

    MELO_LISTENER(mserver->on_supervisor, mserver, index);

    // check and close timeout-ed connections
    if (mserver->config.conn_timeout_ms > 0)
    {
        _check_timeout_clients(mserver);
    }
}


static void _disconnect_client(uv_stream_t* uvclient)
{
    melo_server_conn_t * conn = (melo_server_conn_t *) uvclient->data;
    assert(conn && ((uv_stream_t*)&conn->uvclient == uvclient));
    uv_read_stop(uvclient);
    assert(conn->mserver);

    MELO_LISTENER(conn->mserver->on_conn_closing, conn->mserver, conn);

    uv_close((uv_handle_t*)uvclient, _cb_after_close_connection);
}


/**
 * check timeout client and disconnect.
 */
static void _check_timeout_clients(melo_server_t * mserver)
{
    int conn_timeout = mserver->config.conn_timeout_ms; // in milliseconds

    MELO_ASSERT(conn_timeout > 0, "invalid timeout");

    struct lh_entry *e, *tmp;
    lh_foreach_safe(mserver->conns, e, tmp)
    {
        melo_server_conn_t * conn = (melo_server_conn_t *) e->k;
        if(uv_now(mserver->uvloop) - conn->last_comm_time > conn_timeout)
        {
            MELO_INFO(mserver->config.log_out,
                    "[melo-server] %s close connection %p for its long time silence\n",
                    mserver->config.name,
                    &conn->uvclient);
            _disconnect_client((uv_stream_t*) &conn->uvclient); // will delete connection
        }
        else
        {
            break; //tail is new than header
        }
    }
}



