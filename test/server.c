#include "../melo_server.h"

// Author: Liigo <com.liigo@gmail.com>
typedef struct tcp_server_s
{
    int count;
    long recvByte;
}tcp_server_t;

tcp_server_t server = {0};

static void on_conn_ok(melo_server_t * mserver, melo_server_conn_t * conn)
{
    char ip[40] = {0};
    int port = 0;
    
    server.count++;

    melo_get_tcp_ip_port(&conn->uvclient, ip, sizeof(ip), &port);
    printf("Connected from %s:%d", ip, port);
}

static void on_conn_fail(melo_server_t * mserver, melo_server_conn_t * conn)
{

}

static void on_conn_closing(melo_server_t * mserver, melo_server_conn_t * conn)
{
    puts("on_conn_closing");
}

static void on_conn_close(melo_server_t * mserver, melo_server_conn_t * conn)
{
    puts("on_conn_close");
    server.count--;
}

static void on_supervisor(melo_server_t * mserver, unsigned int index)
{
    printf("There are %d clients connected, recieved %lu bytes in %d ms\n", 
            server.count, server.recvByte, mserver->config.supervisor_interval_ms);

    server.recvByte = 0;
}

static void on_recv(melo_server_t * mserver, melo_server_conn_t * conn, void* data, ssize_t datalen)
{
    char ip[40] = {0};
    int port = 0;

    server.recvByte += datalen;
    
    melo_get_tcp_ip_port(&conn->uvclient, ip, sizeof(ip), &port);
    //printf("receive %d bytes from %s:%d\n", datalen, ip, port);
}

void main(void)
{
    uv_loop_t* loop = uv_default_loop();
    melo_server_t server;

    melo_server_init(&server);

    melo_server_config_t config = melo_server_default_config(&server);
    config.ip = "127.0.0.1";
    config.port = 8001;
    config.supervisor_interval_ms = 10 * 1000;

    melo_server_reg_listener(&server, UVX_S_ON_CONN_OK, on_conn_ok);
    melo_server_reg_listener(&server, UVX_S_ON_CONN_FAIL, on_conn_fail);
    melo_server_reg_listener(&server, UVX_S_ON_CONN_CLOSING, on_conn_closing);
    melo_server_reg_listener(&server, UVX_S_ON_CONN_CLOSE, on_conn_close);
    melo_server_reg_listener(&server, UVX_S_ON_SUPERVISOR, on_supervisor);
    melo_server_reg_listener(&server, UVX_S_ON_RECV, on_recv);

    melo_server_start(&server, loop, config);
    uv_run(loop, UV_RUN_DEFAULT);
}

