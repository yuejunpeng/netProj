#include "tju_tcp.h"

// 创建并初始化TCP socket
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = NULL;
    sock->sending_len = 0;

    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = NULL;
    sock->received_len = 0;

    pthread_mutex_init(&(sock->received_len_lock), NULL);
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){//初始化成功返回0
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    // sock->window.wnd_recv = NULL;
    // sock->window.wnd_recv = NULL;

    sock->window.wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));
    sock->window.wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
    
    sock->window.wnd_send->base = 1;//new
    sock->window.wnd_send->nextseq = 1;//new
    sock->window.wnd_send->rwnd = TCP_RECVWN_SIZE ;//new new new
    sock->window.wnd_send->rto = 0.0001;
    sock->window.wnd_send->ertt = 0.0001;
    sock->window.wnd_send->rttvar = 0.0001;

    sock->window.wnd_send->ack_cnt = 1;

    printf("初始化时，rto为%f\n",sock->window.wnd_send->rto);

    sock->window.wnd_recv->expect_seq = 1; // newnewnew

    

    printf("socket创建完成！\n");

    return sock;
}


// 绑定监听的地址
int tju_bind(tju_tcp_t *sock, tju_sock_addr bind_addr) {
    // int hashval = cal_hash(bind_addr.ip, bind_addr.port, 0, 0);

    // // 检测端口是否被占用
    // int bind_flag = 0; //端口被占用
    // for (int i = 0; i < MAX_SOCK; i++) {
    //     if (bind_socks[i]->port == bind_addr.port) {
    //         bind_flag = 1;
    //         break;
    //     }
    // }

    // // 如果未被占用
    // if (!bind_flag) {
    //     sock->bind_addr = bind_addr;

    //     // 存入b_hash
    //     bind_socks[hashval]->port = sock->bind_addr.port;
    //     bind_socks[hashval]->sock = (tju_tcp_t*) malloc(sizeof(tju_tcp_t));
    //     bind_socks[hashval]->sock = sock;
    //     printf("bind成功!\n");
    // }

    // // 如果被占用
    // if (bind_flag) {
    //     perror("ERROR: 端口被占用，bind失败！\n");
    //     exit(-1);
    // }

    // debug
    sock->bind_addr = bind_addr;
    printf("bind成功（debug）\n");

    return 0;
}


// 被动打开，监听bind的地址
int tju_listen(tju_tcp_t* sock){
    // 进入listen状态
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock; //注册该socket到l_hash
    printf("listen完成！\n");
    return 0;
}


// 接受连接, 返回与客户端通信用的socket
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    int flag = 0; //全连接列表里有socket
    int index; // 已建立连接的socket索引
    tju_tcp_t* new_conn; //已经建立连接的socket

    // 阻塞等待全连接列表不为空
    while (TRUE) {
        for (int i = 0; i < MAX_SOCK; i++) {
            if (full_conn_socks[i] != NULL) {
                flag = 1;
                index = i;
                break;
            }
        }

        if (flag) {
            new_conn = full_conn_socks[index];
            full_conn_socks[index] = NULL;
            break;
        }
    }

    printf("accept完成！\n");

    return new_conn;
}


// 连接到服务端
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){
    // 设置对方地址
    sock->established_remote_addr = target_addr;

    // 设置本地IP
    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");

    // // 分配端口
    // while (TRUE) {
    //     uint16_t port_tmp = generate_port(); // 随机分配一个port
    //     int hashval = cal_hash(local_addr.ip, port_tmp, target_addr.ip, target_addr.port);

        // // 检测端口是否被占用
        // int bind_flag = 0; //端口被占用
        // for (int i = 0; i < MAX_SOCK; i++) {
        //     if (bind_socks[i]->port == port_tmp) {
        //         bind_flag = 1;
        //         break;
        //     }
        // }

        // // 如果未被占用
        // if (!bind_flag) {
        //     local_addr.port = port_tmp;

        //     // 存入b_hash
        //     bind_socks[hashval]->port = port_tmp;
        //     bind_socks[hashval]->sock = (tju_tcp_t*) malloc(sizeof(tju_tcp_t));
        //     bind_socks[hashval]->sock = sock;
        //     printf("分配端口成功!\n");
        //     break;
        // }

        // // 如果被占用
        // if (bind_flag) {
        //     printf("端口被占用，重新分配端口！\n");
        // }
    // }

    // debug
    uint16_t port_tmp = generate_port(); // 随机分配一个port
    local_addr.port = port_tmp;
    printf("分配端口成功（debug）!\n");

    // 设置本地地址
    sock->established_local_addr = local_addr;

    // 将socket放入e_hash
    int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
                sock->established_remote_addr.ip, sock->established_remote_addr.port);
    established_socks[hashval] = sock;

    // 发送SYN
    send_syn(sock, 1, 0, TCP_RECVWN_SIZE);
    printf("第一次握手完成\n");

    // 进入SYN_SENT状态
    sock->state = SYN_SENT;
    printf("进入SYN_SENT状态\n");

    // 阻塞等待进入ESTABLISHED状态，状态的改变在tju_handle_packet
    while (sock->state != ESTABLISHED);

    sleep(1);

    printf("connect完成！\n");
    
    return 0;
}


/* 发送数据
len是包体长度(不包包头)，buffer是待发送消息的起始指针*/
int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    
    printf("开始send\n");
    while(pthread_mutex_lock(&(sock->send_lock)) != 0); // 加锁
    printf("send_lock已上锁\n");

    char* data =(char*) malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = sock->window.wnd_send->nextseq;
    uint16_t plen = DEFAULT_HEADER_LEN + len;
    uint16_t adv_win = 0;

    // 阻塞等待，用于流量控制
    while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd);

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, adv_win, 0, data, len);
    printf("包的序号是%u\n",get_seq(msg));
    // 更新next_seq
    sock->window.wnd_send->nextseq += len;
    printf("sock->window.wnd_send->nextseq更新为%u",sock->window.wnd_send->nextseq);

    sendToLayer3(msg, plen);
    
    printf("尝试开启计时器时，当前RTO为%f\n",sock->window.wnd_send->rto);
    reset_my_timer(sock->window.wnd_send->rto,sock,seq);// 尝试开启计时器
    
    printf("尝试开启计时器，成功，seq为%u\n",seq);
    // 发过的包存入重传缓存区resend_buf
    sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base] = malloc(sizeof(pkt_data_send));

    sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->msg = malloc(plen);
    memcpy(sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->msg, msg, plen);

    // sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->msg = msg;
    sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->plen = plen;
    sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->data_len = len;

    
    printf("装进重传缓冲区的包的序号为：%u\n",get_seq(sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->msg));
    char hostname[8];
    gethostname(hostname, 8);

    printf("此处的hostname为%s\n",hostname);

    free(data);     free(msg);
    data = NULL;    msg = NULL;
    
    printf("发送数据成功！send结束\n");

    pthread_mutex_unlock(&(sock->send_lock)); // 解锁

    printf("send_lock已解锁\n");
    
    
    // while(pthread_mutex_lock(&(sock->send_lock)) != 0); // 加锁
    // // 给发送缓冲区分配空间
    // if(sock->sending_buf == NULL) {
    //     sock->sending_buf =(char*) malloc(len);
    // }
    // else {
    //     sock->sending_buf = realloc(sock->sending_buf, sock->sending_len + len);
    // }

    // // 把包体放进发送缓冲区
    // memcpy(sock->sending_buf + sock->sending_len, buffer, len);
    // sock->sending_len += len;


    // char* msg;
    // uint32_t seq = sock->window.wnd_send->nextseq;
    // uint16_t plen = DEFAULT_HEADER_LEN + len;
    // uint16_t adv_win = 0;

    // // 阻塞等待，用于流量控制
    // while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd);
    
    // // buf + nextseq - 1，一定要减一！！！
    // msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
    //           DEFAULT_HEADER_LEN, plen, NO_FLAG, adv_win, 0, sock->sending_buf + sock->window.wnd_send->nextseq - 1, len);
    
    // // 更新next_seq
    // sock->window.wnd_send->nextseq += plen - len;

    // sendToLayer3(msg, plen);
    
    // reset_my_timer(sock->window.wnd_send->rto,sock,seq);// 尝试开启计时器

    // // 存入重传缓存区resend_buf
    // sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base] = malloc(sizeof(pkt_data_send));
    // sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->msg = msg;
    // sock->window.wnd_send->resend_buf[seq - sock->window.wnd_send->base]->plen = plen;
    
    // printf("发送数据成功！send结束\n");

    // pthread_mutex_unlock(&(sock->send_lock)); // 解锁
    
    return 0;
}


/*接收数据
len是包体长度(不包包头)，buffer是待发送消息的起始指针*/
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    printf("开始recv\n");

    // 没有消息就阻塞
    while(sock->received_len <= 0)
        sleep(0.1);
    // received_len 加锁
    //while(pthread_mutex_lock(&(sock->received_len_lock)) != 0); 
    

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
    //printf("加锁\n");

    int read_len = 0; // 实际读取的长度
    /*从中读取len长度的数据
    len: 要读取的数据长度
    received_len：接收缓冲区的长度*/
    if (sock->received_len >= len){ //不能从缓冲区一次读取
        read_len = len;
    }
    else{
        read_len = sock->received_len; // 从缓冲区读取所有数据
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len); // 新开一个剩下长度的空间
        // 把剩余数据放到新开的空间上
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);

        free(sock->received_buf); // 释放原有的空间
        sock->received_len -= read_len;
        sock->received_buf = new_buf; // 调整指针
    }
    else{
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }

    
    // 解锁
    //pthread_mutex_unlock(&(sock->received_len_lock)); // 解锁
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁
    //printf("解锁\n");
    printf("接收数据成功！recv结束\n");

    return 0;
}


/*
处理数据包
*/
int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    // 获取报文头部信息
    uint16_t pkt_src_port = get_src(pkt);
    uint16_t pkt_dst_port = get_dst(pkt);
    uint32_t pkt_seq = get_seq(pkt);
    uint32_t pkt_ack = get_ack(pkt);
    uint16_t pkt_hlen = get_hlen(pkt);
    uint16_t pkt_plen = get_plen(pkt);
    uint8_t pkt_flags = get_flags(pkt);
    uint16_t pkt_adv_win = get_advertised_window(pkt);
    
    uint16_t data_len = pkt_plen - DEFAULT_HEADER_LEN; // 包体长度
    printf("flag为%u\n",pkt_flags);
    printf("ack为%u\n",pkt_ack);

    printf("新收到包的data_len为%u\n",data_len);

    //printf("已收到包，开始handle\n");

    // 如果在LISTEN状态下收到SYN（三次握手）
    if (sock->state==LISTEN && pkt_flags==SYN_FLAG_MASK) {
        printf("处于listen状态下，收到SYN\n");
        // 创建一个new_socket
        tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));

        //初始化new_conn
        new_conn->state = LISTEN;

        pthread_mutex_init(&(new_conn->send_lock), NULL);
        new_conn->sending_buf = NULL;
        new_conn->sending_len = 0;

        pthread_mutex_init(&(new_conn->recv_lock), NULL);
        new_conn->received_buf = NULL;
        new_conn->received_len = 0;
        
        if(pthread_cond_init(&new_conn->wait_cond, NULL) != 0){//初始化成功返回0
            perror("ERROR condition variable not set\n");
            exit(-1);
        }

        new_conn->window.wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));
        new_conn->window.wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
        new_conn->window.wnd_send->base = 1;//new
        new_conn->window.wnd_send->nextseq = 1;//new
        new_conn->window.wnd_send->rwnd = TCP_RECVWN_SIZE ;//new new new
        new_conn->window.wnd_recv->expect_seq = 1; // newnewnew

        // new_conn->window.wnd_send->ack_cnt = 1;  server不需要，client需要

        new_conn->window.wnd_send->rto = 0.0001;
        new_conn->window.wnd_send->ertt = 0.0001;
        new_conn->window.wnd_send->rttvar = 0.0001;

        
        printf("new_conn创建完成！\n");
        printf("第一次握手完成！\n");

        // 设置new_conn对方地址
        tju_sock_addr remote_addr;
        remote_addr.ip = inet_network("10.0.0.2");  //具体的IP地址
        remote_addr.port = pkt_src_port;
        new_conn->established_remote_addr = remote_addr;

        // 设置new_conn本机地址
        tju_sock_addr local_addr;
        local_addr.ip = sock->bind_addr.ip;  //具体的IP地址
        local_addr.port = sock->bind_addr.port;
        new_conn->established_local_addr = local_addr;

        // 将new_conn放到e_hash和半连接列表中
        int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
        established_socks[hashval] = new_conn;
        semi_conn_socks[hashval] = new_conn;

        // 发送SYN,ACK
        send_syn_ack(new_conn, 1, pkt_ack+1, TCP_RECVWN_SIZE);

        // 更改new_conn状态为SYN_RECV
        new_conn->state = SYN_RECV;
        printf("进入SYN_RECV状态\n");

        printf("第二次握手完成！\n");

        
    }


    // 如果在SYN_SENT状态下收到SYN,ACK（三次握手）(client)
    if (sock->state==SYN_SENT && pkt_flags==SYN_FLAG_MASK+ACK_FLAG_MASK) {
        printf("处于SYN_SENT状态下，收到SYN,ACK\n");
        printf("第二次握手完成！\n");

        // 发送ACK
        send_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 改变状态
        sock->state = ESTABLISHED;
        printf("进入ESTABLISHED状态！\n");

        printf("第三次握手完成！\n");

        printf("准备新建计时器线程\n");
         // 开启new_conn的计时器线程
        pthread_t timer_my_thread_id = 1002;
        int rst = pthread_create(&timer_my_thread_id, NULL, timer_my_thread, (void*)(sock));
        if (rst<0){
            printf("ERROR open timer thread\n");
            exit(-1); 
        }
        pthread_mutex_init(&(sock->timer.timer_seq_lock), NULL);

        printf("计时器线程建立成功\n");
    }


    // 如果在SYN_RECV状态下收到ACK（三次握手） (server)
    if (sock->state==SYN_RECV && pkt_flags==ACK_FLAG_MASK) {
        printf("处于SYN_RECV状态下，收到ACK\n");
        
        // 将new_conn从半连接列表取出，放到全连接列表
        int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
                sock->established_remote_addr.ip, sock->established_remote_addr.port);
        semi_conn_socks[hashval] = NULL;
        full_conn_socks[hashval] = sock;

        sock->state = ESTABLISHED;
        printf("进入ESTABLISHED状态\n");

        printf("第三次握手完成！\n");

        printf("准备新建计时器线程\n");
         // 开启new_conn的计时器线程
        pthread_t timer_my_thread_id = 1003;
        int rst = pthread_create(&timer_my_thread_id, NULL, timer_my_thread, (void*)(sock));
        if (rst<0){
            printf("ERROR open timer thread\n");
            exit(-1); 
        }
        pthread_mutex_init(&(sock->timer.timer_seq_lock), NULL);
        printf("计时器线程建立成功\n");
    }

    
    // 如果在SYN_SENT状态下收到SYN（同时打开）
    if (sock->state==SYN_SENT && pkt_flags==SYN_FLAG_MASK) {
        printf("处于SYN_SENT状态下，收到SYN（同时打开）\n");

        // 发送SYN,ACK
        send_syn_ack(sock, 1, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入SYN_RECV状态
        sock->state = SYN_RECV;
        printf("进入SYN_RECV\n");
    }


    // 如果在SYN_SENT状态时超时或者close（缺timer，待实现）
    // 进入close状态


    // 如果在ESTABLISHED状态下收到FIN（四次挥手）
    if (sock->state==ESTABLISHED && pkt_flags==FIN_FLAG_MASK) {
        printf("处于ESTABLISHED状态下，收到FIN\n");
        printf("第一次挥手完成\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入CLOSE_WAIT状态
        sock->state = CLOSE_WAIT;
        printf("进入CLOSE_WAIT状态\n");

        printf("第二次挥手完成\n");

        // 等待数据发送完
        sleep(1); // 这里要研究研究
        printf("等待数据发送完\n");

        // 发送FIN
        send_fin(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入LAST_ACK
        sock->state = LAST_ACK;
        printf("进入LAST_ACK状态\n");

        printf("第三次挥手完成！\n");
    }


    // 如果在FIN_WAIT_1状态收到FIN,ACK（四次挥手）
    if (sock->state==FIN_WAIT_1 && pkt_flags==FIN_FLAG_MASK+ACK_FLAG_MASK) {
        printf("处于FIN_WAIT_1状态下，收到FIN,ACK\n");

        sock->state = FIN_WAIT_2;

        printf("进入FIN_WAIT_2\n");
        printf("第二次挥手完成\n");
    }


    // 如果在FIN_WAIT_2状态收到FIN（四次挥手）
    if (sock->state==FIN_WAIT_2 && pkt_flags==FIN_FLAG_MASK) {
        printf("处于FIN_WAIT_2状态下，收到FIN\n");
        printf("第三次挥手完成\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入TIME_WAIT状态
        sock->state = TIME_WAIT;
        printf("进入TIME_WAIT状态\n");

        printf("第四次挥手完成\n");

        // 等待2MSS后关闭
        sleep(1); // 还需进一步研究
        sock->state = CLOSED;

        printf("等待2MSS后，进入CLOSED状态\n");
    }


    // 如果在LAST_ACK状态收到FIN,ACK（四次挥手）
    if (sock->state==LAST_ACK && pkt_flags==FIN_FLAG_MASK+ACK_FLAG_MASK) {
        printf("处于LAST_ACK状态下，收到FIN,ACK\n");

        sock->state = CLOSED;

        printf("进入CLOSED状态\n");
        printf("第四次挥手完成\n");
    }


    // 如果在FIN_WAIT_1状态收到FIN（同时关闭）
    if (sock->state==FIN_WAIT_1 && pkt_flags==FIN_FLAG_MASK) {
        printf("处于FIN_WAIT_1状态下，收到FIN（同时关闭）\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入CLOSING状态
        sock->state = CLOSING;
        printf("进入CLOSING\n");
    }


    // 如果在CLOSING状态收到FIN,ACK（同时关闭）
    if (sock->state==CLOSING && pkt_flags==FIN_FLAG_MASK+ACK_FLAG_MASK) {
        printf("处于CLOSING状态下，收到FIN,ACK（同时关闭）\n");

        // 进入TIME_WAIT状态
        sock->state = TIME_WAIT;
        printf("进入TIME_WAIT状态\n");

        // 等待2MSS后关闭
        sleep(1); // 还需进一步研究
        sock->state = CLOSED;
        printf("等待2MSS后，进入CLOSED状态\n");
    }




    // 如果收到普通数据包
    if(pkt_flags==NO_FLAG) {
        printf("收到普通的数据包\n");
        // 如果包在接收窗口内（pkt_seq < expect_seq+adv_win）
        if(pkt_seq < sock->window.wnd_recv->expect_seq+TCP_RECVWN_SIZE-sock->received_len && pkt_seq >= sock->window.wnd_recv->expect_seq) 
        {
            // 如果是期望按序的包
            if(pkt_seq == sock->window.wnd_recv->expect_seq) {
                // 把收到的数据放到接收缓冲区
                while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
                // 给接收缓冲区分配空间
                if(sock->received_buf == NULL){
                    sock->received_buf = (char*) malloc(data_len);
                }
                else {
                    sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
                }

                //received_len锁 加锁
                //while(pthread_mutex_lock(&(sock->received_len_lock)) != 0); 


                // 把收到的数据（包体）放到接收缓冲区
                memcpy(sock->received_buf + sock->received_len , pkt + DEFAULT_HEADER_LEN  , data_len);
                sock->received_len += data_len;

                printf("数据包按顺序到达，expect_seq=%d, pkt_seq=%d, 已放入接收缓冲区，当前时间为%ld\n", 
                        sock->window.wnd_recv->expect_seq, pkt_seq, time(NULL));
                // printf("接收到的数据包为：%s\n",sock->received_buf + sock->received_len);

                // 检查disorder_buf[]中的元素是否可以取出
                int len_cnt = 0;// expect_seq可能要多加一些。

                if(sock->window.wnd_recv->disorder_buf[16]!=NULL)
                    printf("sock->window.wnd_recv->disorder_buf[16]->data_len为%u\n",sock->window.wnd_recv->disorder_buf[16]->data_len);
                
                //pkt_data* nextpkt = sock->window.wnd_recv->disorder_buf[pkt_seq + data_len - sock->window.wnd_recv->expect_seq];// 索引就是data_len
                
                // sock->window.wnd_recv->disorder_buf[data_len];

                while(sock->window.wnd_recv->disorder_buf[data_len + len_cnt] != NULL) {
                    //取出disorder_buf[]的第一个能取到的包，放到received_buf中
                    
                    // nextpkt->data_len
                    printf("index为%u\n",pkt_seq + data_len - sock->window.wnd_recv->expect_seq);
                    // 把包体（不包含头部）放进接收缓冲区，乱序结构体数组之后一并清理。
                    sock->received_buf = realloc(sock->received_buf, sock->received_len + sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len); // 重新分配缓冲区的大小
                    memcpy(sock->received_buf + sock->received_len,  sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data, sock->window.wnd_recv->disorder_buf[data_len+ len_cnt]->data_len);

                    
                    
                    
                    sock->received_len += sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len;
                    printf("sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len为%u\n",
                            sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len);

                    // received_len 解锁
                    //pthread_mutex_unlock(&(sock->received_len_lock)); // 解锁

                    // sock->window.wnd_recv->expect_seq++;
                    len_cnt += sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len;
                    printf("len_cnt更新为%d\n",len_cnt);
                    // printf("nextpkt->data_len更新为%u\n",sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data_len);

                    // free(sock->window.wnd_recv->disorder_buf[data_len + len_cnt]->data);
                    // free(sock->window.wnd_recv->disorder_buf[data_len + len_cnt]);// 释放空间
                    
                    // nextpkt = sock->window.wnd_recv->disorder_buf[pkt_seq + data_len + len_cnt - sock->window.wnd_recv->expect_seq];
                    // 前两项不变，第三项一直在加，最后一项不变。
                }

                // 更新expect_seq
                sock->window.wnd_recv->expect_seq += (data_len + len_cnt);
                printf("expect_seq更新为%d\n",sock->window.wnd_recv->expect_seq);

                // disorder_buf[]整体左移，左移长度为 data_len + len_cnt,即sock->window.wnd_recv->expect_seq。
                for(int i = 1 ; i < TCP_RECVWN_SIZE - data_len -len_cnt ; i++ ) {
                    sock->window.wnd_recv->disorder_buf[i] = sock->window.wnd_recv->disorder_buf[i+len_cnt+data_len];
                }
                
                for(int i = TCP_RECVWN_SIZE - len_cnt - data_len;i<TCP_RECVWN_SIZE;i++)
                    sock->window.wnd_recv->disorder_buf[i] = NULL;
                
            }

            // 如果是乱序的包
            else {
                while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
                
                printf("收到乱序到达的包，expect_seq=%d, pkt_seq=%d,可能会存入disorder_buf[]中\n",sock->window.wnd_recv->expect_seq, pkt_seq);
                // 将乱序的包结构体存入disorder_buf[]，索引是pkt_seq-expect_seq
                int index = pkt_seq - sock->window.wnd_recv->expect_seq;
                printf("index为%d\n",index);

                if(sock->window.wnd_recv->disorder_buf[index] == NULL)
                {
                    sock->window.wnd_recv->disorder_buf[index] = malloc(sizeof(pkt_data));
                
                    sock->window.wnd_recv->disorder_buf[index]->data = malloc(data_len);
                    memcpy(sock->window.wnd_recv->disorder_buf[index]->data, pkt + DEFAULT_HEADER_LEN, data_len);
                
                    sock->window.wnd_recv->disorder_buf[index]->data_len = data_len;
                    printf("data_len为%u,sock->window.wnd_recv->disorder_buf[%d]->data_len为%u\n"
                        ,data_len, index, sock->window.wnd_recv->disorder_buf[index]->data_len);
                    sock->window.wnd_recv->disorder_buf[index]->seq = pkt_seq;
                }
                
            }
            
            // 发送ACK
            //adv_win = TCP_RECVWN_SIZE - sock->received_len
            send_ack(sock, 0, sock->window.wnd_recv->expect_seq, TCP_RECVWN_SIZE - sock->received_len);
            
            pthread_mutex_unlock(&(sock->recv_lock)); // 解锁
        }
        
        // 如果包不在接收窗口内
        else {
            // 忽略
            while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁


            printf("收到的包seq为%u,不在接收窗口内，丢弃\n",pkt_seq);
            printf("发送ACK\n");
            send_ack(sock, 0, sock->window.wnd_recv->expect_seq, TCP_RECVWN_SIZE - sock->received_len);
            
            pthread_mutex_unlock(&(sock->recv_lock)); // 解锁
            return 0;
        }
    }
    
    printf("base为%u，nextseq为%u\n",sock->window.wnd_send->base,sock->window.wnd_send->nextseq);
    // 发送方收正常的ACK。第四个判断条件要加等号！
    if(sock->state==ESTABLISHED && pkt_flags==ACK_FLAG_MASK && pkt_ack>=sock->window.wnd_send->base && pkt_ack <= sock->window.wnd_send->nextseq) 
    {
        printf("收到了ACK！ACK号为%d\n",pkt_ack);
        
        // 更新rwnd
        sock->window.wnd_send->rwnd = pkt_adv_win;
        printf("rwnd窗口值更新为%d\n", sock->window.wnd_send->rwnd);
        
        // 重传缓冲区整体左移:pkt_ack - 1 - sock->window.wnd_send->base + data_len
        int move_len = pkt_ack - sock->window.wnd_send->base;
        // if(sock->window.wnd_send->resend_buf[pkt_ack - 1] == NULL)
        // {
        //     move_len = pkt_ack - 1 - sock->window.wnd_send->base;
        // }
        // else
        // {
        //     move_len = pkt_ack - 1 - sock->window.wnd_send->base + sock->window.wnd_send->resend_buf[pkt_ack - 1]->data_len;// 移动长度
        // 
        printf("movelen计算完成\n");
        
        
        // 删除重传缓冲区。最后一个字节的序号 seq = pkt_ack - 1
        // for(int i = 0 ; i < (int)(pkt_ack) - (int)(sock->window.wnd_send->base) ; i++)
        // {
        //     if(sock->window.wnd_send->resend_buf[i]->msg !=NULL)
        //         free(sock->window.wnd_send->resend_buf[i]->msg);
            
        //     if(sock->window.wnd_send->resend_buf[i] !=NULL)
        //         free(sock->window.wnd_send->resend_buf[i]);
        // }

        printf("删除重传缓冲区前部分\n");

        
        for(int i = 0 ; i < TCP_RECVWN_SIZE - move_len ; i++)
        {
            sock->window.wnd_send->resend_buf[i] = sock->window.wnd_send->resend_buf[i+move_len];
        }
        printf("左移完成，下面对剩余部分进行赋值\n");
        for(int i = TCP_RECVWN_SIZE - move_len ; i < TCP_RECVWN_SIZE;i++)
        {
            sock->window.wnd_send->resend_buf[i] = NULL;
        }

        printf("开始更新base\n");
        // 滑动窗口，更新base值
        sock->window.wnd_send->base = pkt_ack;
        printf("base更新为%d\n", sock->window.wnd_send->base);

        // 计算ERTT,以及后续的变量


        struct timeval tv;
        gettimeofday(&tv, NULL);

        double srtt = (double) (tv.tv_sec + tv.tv_usec / 1000000.00) - sock->timer.start_time;
        printf("新获得的srtt为%f\n",srtt);
        sock->window.wnd_send->ertt = sock->window.wnd_send->ertt * (1 - a) + a * srtt;
        printf("ertt更新为%f\n",sock->window.wnd_send->ertt);
        sock->window.wnd_send->rttvar = sock->window.wnd_send->rttvar * (1 - b) + b * abs(sock->window.wnd_send->ertt - srtt);
        printf("rttvar更新为%f\n",sock->window.wnd_send->rttvar);
        sock->window.wnd_send->rto = sock->window.wnd_send->ertt + 4 * sock->window.wnd_send->rttvar;
        printf("rto更新为%f\n",sock->window.wnd_send->rto);

        if(pkt_ack == sock->timer.seq)
        {
            sock->window.wnd_send->ack_cnt++;
            if(sock->window.wnd_send->ack_cnt == 3)
            {
                // 快速重传
                char* msg = sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base]->msg;
                uint16_t plen = sock->window.wnd_send->resend_buf[sock->timer.seq - sock->window.wnd_send->base]->plen;
                while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd);
                sendToLayer3(msg, plen);// 不用create packet，直接发，因为msg本来就是打好的包。
                printf("快速重传\n");
                sock->window.wnd_send->ack_cnt =1;
                
                
            }
            return 0;// 冗余的ACK就不重启计时器了。

        }
        
        // 重启计时器
        reset_my_timer_f(sock->window.wnd_send->rto, sock, pkt_ack);

        // 这里不需要重传。重传只有超时重传和快速重传。
        

        // // 如果是按序ACK
        // if(pkt_ack == sock->window.wnd_send->base) {
        //     // 更新base，滑动窗口
        //     sock->window.wnd_send->base += data_len;

        //     // 删除发送缓冲区,待补充
        //     for(int i=0; i<TCP_RECVWN_SIZE ; i++)
        //     {
        //         if(sock->window.wnd_send->resend_buf[i])
        //             sock->window.wnd_send->resend_buf
        //     }


        //     // 计算ERTT,以及后续的变量
        //     int srtt = time(NULL) - sock->timer.start_time;
        //     sock->window.wnd_send->ertt = sock->window.wnd_send->ertt * (1 - a) + a * srtt;
        //     sock->window.wnd_send->rttvar = sock->window.wnd_send->rttvar * (1 - b) + b * abs(sock->window.wnd_send->ertt - srtt);
        //     sock->window.wnd_send->rto = sock->window.wnd_send->ertt + 4 * sock->window.wnd_send->rttvar;
            
            
            
        //     // 重启计时器
        //     reset_my_timer(sock->window.wnd_send->rto, sock, pkt_ack);


        // }
        // // 如果不是按序ACK
        // else {
        //     //重传
        //     char* msg = sock->window.wnd_send->resend_buf[pkt_ack]->msg;
        //     uint16_t plen = sock->window.wnd_send->resend_buf[pkt_ack]->plen;

        //     // 阻塞等待，用于流量控制
        //     while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd);
            
        //     sendToLayer3(msg, plen);
        // }
    }




    // while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
    // // 给接收缓冲区分配空间
    // if(sock->received_buf == NULL) { 
    //     sock->received_buf = malloc(data_len);
    // }
    // else {
    //     sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    // }
    // // 把收到的数据（包体）放到接收缓冲区
    // memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    // sock->received_len += data_len;
    // pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    printf("handle packet结束\n");

    return 0;
}


/*
关闭 TCP socket
*/
int tju_close (tju_tcp_t* sock){
    printf("开始执行close\n");

    if (sock->state==ESTABLISHED) {
        // 发送FIN
        send_fin(sock, 1, 1, TCP_RECVWN_SIZE);

        // 进入FIN_WAIT_1
        sock->state = FIN_WAIT_1;
        printf("状态由ESTABLISHED变为FIN_WAIT_1\n");

        printf("第一次挥手完成\n");
    }

    if (sock->state==CLOSE_WAIT) {
        // 发送FIN
        send_fin(sock, 1, 1, TCP_RECVWN_SIZE);

        sock->state = LAST_ACK;
        printf("状态由CLOSE_WAIT变为LAST_ACK\n");
    }

    if (sock->state==SYN_RECV) {
        send_fin(sock, 1, 1, TCP_RECVWN_SIZE);
        sock->state = FIN_WAIT_1;
        printf("状态由SYN_RECV变为FIN_WAIT_1\n");
    }

    // 阻塞等待状态为CLOSED
    while(sock->state != CLOSED);

    // 清理占用
    int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
                sock->established_remote_addr.ip, sock->established_remote_addr.port);
    established_socks[hashval] = NULL;
    full_conn_socks[hashval] = NULL;
    free(sock);
    sock = NULL;
    // 还有一些要考虑free......

    printf("close完成！\n");
    return 0;
}


// 随机分配一个port
uint16_t generate_port() {
    srand((unsigned int)time(NULL));
    uint16_t port = rand()%10001 + 3333; //3333 - 13333
    return port;
}


// 发送SYN
void send_syn(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = SYN_FLAG_MASK;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port,
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个SYN！\n");
}


// 发送SYN,ACK
void send_syn_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = SYN_FLAG_MASK+ACK_FLAG_MASK;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个SYN,ACK！\n");
}


// 发送FIN
void send_fin(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = FIN_FLAG_MASK;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个FIN！\n");
}


// 发送FIN,ACK
void send_fin_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = FIN_FLAG_MASK+ACK_FLAG_MASK;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个FIN,ACK！\n");
}


// 发送ACK
void send_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = ACK_FLAG_MASK;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个ACK！\n");
}


