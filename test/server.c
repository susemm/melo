#include "../melo_server.h"

// Author: Liigo <com.liigo@gmail.com>

static void on_recv(melo_server_t * mserver, melo_server_conn_t * conn, void* data, ssize_t datalen)
{
    char ip[40]; int port;
    melo_get_tcp_ip_port(&conn->uvclient, ip, sizeof(ip), &port);
    printf("receive %d bytes from %s:%d\n", datalen, ip, port);
}

static void on_conn_closing(melo_server_t * mserver, melo_server_conn_t* conn)
{
    puts("on_conn_closing");
}

static void on_conn_close(melo_server_t * mserver, melo_server_conn_t* conn)
{
    puts("on_conn_close");
}

void main()
{
    uv_loop_t* loop = uv_default_loop();
    melo_server_t server;

    melo_server_init(&server);

    melo_server_config_t config = melo_server_default_config(&server);
    config.ip = "127.0.0.1";
    config.port = 8001;


    melo_server_reg_listener(&server, UVX_S_ON_RECV, on_recv);
    melo_server_reg_listener(&server, UVX_S_ON_CONN_CLOSING, on_conn_closing);
    melo_server_reg_listener(&server, UVX_S_ON_CONN_CLOSE, on_conn_close);

    melo_server_start(&server, loop, config);
    uv_run(loop, UV_RUN_DEFAULT);
}

