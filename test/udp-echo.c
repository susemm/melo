#include "../melo_udp.h"
#include <string.h>

#ifdef __unix__
    #include <unistd.h>
#else
    #define sleep(n) Sleep((n)*1000)
#endif

/**
 * Run this program in two process,
 * 1st: ./udpecho
 * 2nd: ./udpecho 127.0.0.1
 * then there can send messaget to the other
 * author: Liigo <com.liigo@gmail.com>
 */
static void on_recv(melo_udp_t * mudp, void* data, ssize_t datalen, const struct sockaddr* addr, unsigned int flag)
{
    char ip[16]; int port;
    melo_get_ip_port(addr, ip, sizeof(ip), &port);
    printf("recv: %s  size: %d  from %s:%d \n", (char *)data, datalen, ip, port);
    int x = atoi(data);
    char newdata[16]; sprintf(newdata, "%d", x + 1);
    melo_udp_send_to_addr(mudp, addr, newdata, strlen(newdata) + 1);
    sleep(1);
}

void main(int argc, char** argv)
{
    char* target_ip = NULL;
    uv_loop_t* loop = uv_default_loop();
    melo_udp_t mudp;

    melo_udp_init(&mudp);
    melo_udp_reg_listener(&mudp, MELO_UDP_ON_RECV, on_recv);

    melo_udp_config_t config = melo_udp_default_config(&mudp);

    if(argc > 1)
    {
        target_ip = argv[1];
    }

    if(target_ip == NULL)
    {
        config.ip = "0.0.0.0";
        config.port = 8008;
        melo_udp_start(&mudp, loop, config); // bind to special port
    }
    else
    {
        config.ip = NULL;
        config.port = 0;
        melo_udp_start(&mudp, loop, config); // bind to random port 
        melo_udp_send_to_ip(&mudp, target_ip, 8008, "1", 2); // start sending message
    }

    uv_run(loop, UV_RUN_DEFAULT);
}
