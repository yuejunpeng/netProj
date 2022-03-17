#include "kernel.h"
/*
模拟Linux内核收到一份TCP报文的处理函数
*/
void onTCPPocket(char* pkt){
    // 当我们收到TCP包时 包中 源IP 源端口 是发送方的 也就是我们眼里的 远程(remote) IP和端口
    uint16_t remote_port = get_src(pkt);
    uint16_t local_port = get_dst(pkt);
    // remote ip 和 local ip 是读IP 数据包得到的 仿真的话这里直接根据hostname判断

    char hostname[8];
    gethostname(hostname, 8);

    printf("hostname为%s\n",hostname);
    uint32_t remote_ip, local_ip;
    if(strcmp(hostname,"server")==0){ // 自己是服务端 远端就是客户端
        local_ip = inet_network("10.0.0.1");
        remote_ip = inet_network("10.0.0.2");
    }else if(strcmp(hostname,"client")==0){ // 自己是客户端 远端就是服务端 
        local_ip = inet_network("10.0.0.2");
        remote_ip = inet_network("10.0.0.1");
    }

    int hashval;
    // 根据4个ip port 组成四元组 查找有没有已经建立连接的socket
    hashval = cal_hash(local_ip, local_port, remote_ip, remote_port);
    printf("哈希值为%d\n",hashval);

    // 首先查找已经建立连接的socket哈希表
    if (established_socks[hashval]!=NULL){
        printf("在ehash中找到socket，进入handle_packet\n");
        tju_handle_packet(established_socks[hashval], pkt);
        printf("onTCPPocket结束！\n");
        return;
    }

    // 没有的话再查找监听中的socket哈希表
    hashval = cal_hash(local_ip, local_port, 0, 0); //监听的socket只有本地监听ip和端口 没有远端
    if (listen_socks[hashval]!=NULL){
        tju_handle_packet(listen_socks[hashval], pkt);
        return;
    }

    // 都没找到 丢掉数据包
    printf("找不到能够处理该TCP数据包的socket, 丢弃该数据包\n");
    sleep(1);
    return;
}



/*
以用户填写的TCP报文为参数
根据用户填写的TCP的目的IP和目的端口,向该地址发送数据报
不可以修改此函数实现
*/
void sendToLayer3(char* packet_buf, int packet_len){
    if (packet_len>MAX_LEN){
        printf("ERROR: 不能发送超过 MAX_LEN 长度的packet, 防止IP层进行分片\n");
        return;
    }

    // 获取hostname 根据hostname 判断是客户端还是服务端
    char hostname[8];
    gethostname(hostname, 8);

    struct sockaddr_in conn;
    conn.sin_family      = AF_INET;            
    conn.sin_port        = htons(20218);
    int rst;
    if(strcmp(hostname,"server")==0){
        conn.sin_addr.s_addr = inet_addr("10.0.0.2");
        rst = sendto(BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else if(strcmp(hostname,"client")==0){       
        conn.sin_addr.s_addr = inet_addr("10.0.0.1");
        rst = sendto(BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else{
        printf("请不要改动hostname...\n");
        exit(-1);
    }
}

/*
 仿真接受数据线程
 不断调用server或cliet监听在20218端口的UDPsocket的recvfrom
 一旦收到了大于TCPheader长度的数据 
 则接受整个TCP包并调用onTCPPocket()
*/
void* receive_thread(void* arg){

    char hdr[DEFAULT_HEADER_LEN];
    char* pkt;

    uint32_t plen = 0, buf_size = 0, n = 0;
    int len;

    struct sockaddr_in from_addr;
    int from_addr_size = sizeof(from_addr);

    while(1) {
        // MSG_PEEK 表示看一眼 不会把数据从缓冲区删除
        len = recvfrom(BACKEND_UDPSOCKET_ID, hdr, DEFAULT_HEADER_LEN, MSG_PEEK, (struct sockaddr *)&from_addr, &from_addr_size);
        // 一旦收到了大于header长度的数据 则接受整个TCP包
        
        printf("收到一个包！------------------------------------\n");
        if(len >= DEFAULT_HEADER_LEN){
            plen = get_plen(hdr); 
            pkt = malloc(plen);
            buf_size = 0;
            printf("包的长度大于DEFAULT_HEADER_LEN，开始处理\n");
            while(buf_size < plen){ // 直到接收到 plen 长度的数据 接受的数据全部存在pkt中
                n = recvfrom(BACKEND_UDPSOCKET_ID, pkt + buf_size, plen - buf_size, NO_FLAG, (struct sockaddr *)&from_addr, &from_addr_size);
                buf_size = buf_size + n;
            }
            // 通知内核收到一个完整的TCP报文
            onTCPPocket(pkt);
            free(pkt);
            printf("处理完了一个包\n");   
        }
    }
}



// 计时器线程，始终开启，判断是否超时等
void* timer_my_thread(void* arg){
    tju_tcp_t* sock = (tju_tcp_t*) arg;
    printf("进入计时器线程\n");
    printf("计时器初始化\n");
    // 初始化计时器
    sock->timer.timer_istimeout = 0;
    sock->timer.timer_maxtime = 5.0;
    sock->timer.quit_timer = 0; // 先设置为不在运行中，调用时再置为1.

    
    
    while(sock->timer.quit_timer==0)
    {
        // printf("阻塞，timer还未初始化\n");
        // sleep(0.1);
    };

    while(1)
    {
        // if(sock->timer.quit_timer==0){
        //     continue;
        // }
        struct timeval tv;
        gettimeofday(&tv, NULL);
        sock->timer.now_time =(double) (tv.tv_sec + tv.tv_usec / 1000000.00);
        // printf("当前时间为%f\n",sock->timer.now_time);
        // printf("当前时间获取成功\n");
        // sleep(1);
        // sock->timer.now_time = time(NULL);
        // if (difftime(sock->timer.now_time, sock->timer.start_time)>sock->timer.timer_maxtime){ // 超时。时间单位的问题，之后再讨论
        if ( (sock->timer.now_time - sock->timer.start_time) > sock->timer.timer_maxtime)
        {
            // printf("当前时间为%f\n",sock->timer.now_time);
            printf("当前时间为%f，开始时间为%f，超时间隔为%f\n",sock->timer.now_time,sock->timer.start_time,sock->timer.timer_maxtime);
            printf("超时！seq为%d====================================\n",sock->timer.seq);
            // sleep(10);
            sock->timer.timer_istimeout = 1;// 超时标志位
            

            // 上锁
            while(pthread_mutex_lock(&(sock->timer.timer_seq_lock)) != 0); // 加锁

            printf("计时器线程抢到了timer_seq_lock\n");

            sock->timer.quit_timer = 0; // 计时器停止
            
            
            
            // 超时重传.
            // if(sock->window.wnd_send->rwnd != TCP_RECVWN_SIZE)
            // {

            // }


            // if(sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base] == NULL)
            //     return (void *)(0);

            // // 如果是同一个包出现两次及以上超时
            // if(sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base] == NULL)
            // {
            //     // printf("return\n");
            //     //return (void *)(0);

            //     // 超时间隔加倍
            //     sock->window.wnd_send->rto *=2;

            //     struct timeval tv;
            //     gettimeofday(&tv, NULL);
            //     sock->timer.start_time = (double)(tv.tv_sec + tv.tv_usec / 1000000.0);
            //     printf("超时重新计时，开始时间为%f\n",sock->timer.start_time);

            //     sock->timer.timer_maxtime = sock->window.wnd_send->rto;
            //     sock->timer.timer_istimeout = 0;

            //     sock->timer.seq=sock->timer.seq;
            //     sock->timer.quit_timer = 1;// 计时器开启


            //     // 解锁
            //     pthread_mutex_unlock(&(sock->timer.timer_seq_lock)); // 解锁
            //     printf("计时器线程解开了timer_seq_lock\n");
            //     continue;
            // }

            char* msg = sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base]->msg;
            uint16_t plen = sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base]->plen;
            while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd);
            sendToLayer3(msg, plen);// 不用create packet，直接发，因为msg本来就是打好的包。
            printf("sock->timer.seq为%u，sock->window.wnd_send->base为%u\n",sock->timer.seq,sock->window.wnd_send->base);
            printf("重传包的序号为：%u\n",get_seq(msg));
            printf("超时重传\n");

            // 超时间隔加倍
            sock->window.wnd_send->rto *=2;

            // 重新开启计时器
            // reset_my_timer(sock->window.wnd_send->rto,sock,sock->timer.seq);

            struct timeval tv;
            gettimeofday(&tv, NULL);
            sock->timer.start_time = (double)(tv.tv_sec + tv.tv_usec / 1000000.0);
            printf("超时重新计时，开始时间为%f\n",sock->timer.start_time);

            sock->timer.timer_maxtime = sock->window.wnd_send->rto;
            sock->timer.timer_istimeout = 0;

            sock->timer.seq=sock->timer.seq;
            sock->timer.quit_timer = 1;// 计时器开启


            // 解锁
            pthread_mutex_unlock(&(sock->timer.timer_seq_lock)); // 解锁
            printf("计时器线程解开了timer_seq_lock\n");
        }
    }
}

// 尝试开启（重启）计时器，传入当前的RTO，发送包的时候用。
void reset_my_timer(double maxtime, tju_tcp_t* sock, uint32_t seq){
    if(sock->timer.quit_timer==0)// 如果计时器处于关闭状态，则开启，否则不作为。
    {
        // sock->timer.start_time = time(NULL);
        
        printf("开启（重启）计时器！\n");
        printf("已开启计时器时，当前RTO为%f\n",sock->window.wnd_send->rto);

        struct timeval tv;
        gettimeofday(&tv, NULL);
        sock->timer.start_time = (double)(tv.tv_sec + tv.tv_usec / 1000000.0);
        printf("计时开始时间为%f\n",sock->timer.start_time);

        sock->timer.timer_maxtime = maxtime;
        sock->timer.timer_istimeout = 0;
        // sock->timer.sock=sock;// 貌似不需要
        
        // 上锁
        while(pthread_mutex_lock(&(sock->timer.timer_seq_lock)) != 0); // 加锁
        printf("timer_seq_lock上锁\n");
        sock->timer.seq=seq;
        sock->timer.quit_timer = 1;// 计时器开启

        // 解锁
        pthread_mutex_unlock(&(sock->timer.timer_seq_lock)); // 解锁
        printf("timer_seq_lock解锁\n");
    }
    
}

// 强制重启计时器，传入当前的RTO，收到ACK时用。
void reset_my_timer_f(double maxtime, tju_tcp_t* sock, uint32_t seq){

    printf("强制重启计时器！\n");
    printf("之前的seq为%d，现在的seq为%d\n",sock->timer.seq,seq);
    if(seq == sock->timer.seq)
    {
        return;
    }    
    
    
    printf("强制重启计时器时，当前RTO为%f\n",sock->window.wnd_send->rto);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    sock->timer.start_time = (double)(tv.tv_sec + tv.tv_usec / 1000000.0);
    printf("计时开始时间为%f\n",sock->timer.start_time);

    sock->timer.timer_maxtime = maxtime;
    sock->timer.timer_istimeout = 0;
    // sock->timer.sock=sock;// 貌似不需要
    sock->timer.seq=seq;
    sock->timer.quit_timer = 1;// 计时器开启
    
}


/*
 开启仿真, 运行起后台线程

 不论是server还是client
 都创建一个UDP socket 监听在20218端口
 然后创建新线程 不断调用该socket的recvfrom
*/
void startSimulation(){
    // 对于内核 初始化监听socket哈希表和建立连接socket哈希表
    int index;
    for(index=0;index<MAX_SOCK;index++){
        listen_socks[index] = NULL;
        established_socks[index] = NULL;
    }

    // 获取hostname 
    char hostname[8];
    gethostname(hostname, 8);
    // printf("startSimulation on hostname: %s\n", hostname);

    BACKEND_UDPSOCKET_ID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (BACKEND_UDPSOCKET_ID < 0){
        printf("ERROR opening socket");
        exit(-1);
    }

    // 设置socket选项 SO_REUSEADDR = 1 
    // 意思是 允许绑定本地地址冲突 和 改变了系统对处于TIME_WAIT状态的socket的看待方式 
    int optval = 1;
    setsockopt(BACKEND_UDPSOCKET_ID, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    struct sockaddr_in conn;
    memset(&conn, 0, sizeof(conn)); 
    conn.sin_family = AF_INET;
    conn.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = 0.0.0.0
    conn.sin_port = htons((unsigned short)20218);

    if (bind(BACKEND_UDPSOCKET_ID, (struct sockaddr *) &conn, sizeof(conn)) < 0){
        printf("ERROR on binding");
        exit(-1);
    }

    pthread_t thread_id = 1001;
    int rst = pthread_create(&thread_id, NULL, receive_thread, (void*)(&BACKEND_UDPSOCKET_ID));
    if (rst<0){
        printf("ERROR open thread");
        exit(-1); 
    }
    // printf("successfully created bankend thread\n");
    return;
}

int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port){
    // 实际上肯定不是这么算的
    return ((int)local_ip+(int)local_port+(int)remote_ip+(int)remote_port)%MAX_SOCK;
}