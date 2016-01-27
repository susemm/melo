#include "../melo_client.h"

// Author: Liigo <com.liigo@gmail.com>
// Usage:
//   ./client         Send 1 data  per 10 seconds
//   ./client x       Send x datas per 10 seconds
//   ./client x y     Send x datas per y seconds

int bench = 1;
int interval = 10; // Seconds

static void on_heartbeat(melo_client_t * mclient, unsigned int index) 
{
    static char data[] = 
    {
        'u','v','x','-','s','e','r','v','e','r',',','b','y',' ','l','i','i','g','o','\n'
    };
    printf("send %d datas\n", bench);
    for(int i = 0; i < bench; i++) 
    {
        automem_t mem; automem_init(&mem, 32);
        automem_append_voidp(&mem, data, sizeof(data));
        melo_send_mem(&mem, (uv_stream_t*)&mclient->uvclient);
    }
}

void main(int argc, char** argv) 
{
    if(argc > 1) 
    {
        bench = atoi(argv[1]);
    }
    if(argc > 2) 
    {
        interval = atoi(argv[2]); // Seconds
    }

    uv_loop_t* uvloop = uv_default_loop();
    melo_client_t client;
    
    melo_client_init(&client);
    melo_client_reg_listener(&client, UVX_C_ON_SUPERVISOR, on_heartbeat);
    
    melo_client_config_t config = melo_client_default_config(&client);
    config.supervisor_interval_ms = interval * 1000;
    config.ip = "127.0.0.1";
    config.port = 8001;
    melo_client_connect(&client, uvloop, config);

    uv_run(uvloop, UV_RUN_DEFAULT);
}

