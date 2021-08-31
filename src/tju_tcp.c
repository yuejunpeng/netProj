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
    if(sock->sending_buf == NULL) {
        // 初始化的时候sending_buf是一个空指针NULL，在这里给它分配空间
        sock->sending_buf =(char*) malloc(len);
    }
    else {
        sock->sending_buf = realloc(sock->sending_buf, sock->sending_len + len);//重新分配缓冲区的大小
    }

    memcpy(sock->sending_buf + sock->sending_len, buffer, len);// 把包体放进发送缓冲区
    sock->sending_len += len;// 新来的包放进了发送缓冲区，重新计算发送缓冲区的长度。

    pthread_mutex_unlock(&(sock->send_lock)); // 解锁

    char* msg;  // 打包的整个包
    uint32_t seq = sock->window.wnd_send->nextseq;//  seq是序号。nextseq每次发送更新，作为序号的依据。
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    // while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base > sock->window.wnd_send->rwnd);    ＋plen吗？加上吧，讲道理应该加上的。
    while(sock->window.wnd_send->nextseq - sock->window.wnd_send->base + plen > sock->window.wnd_send->rwnd)
    // {
    //     printf("阻塞ing");
    // }
    ;
    // 阻塞，负责流量控制。注意窗口要初始化。

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 0, 0, sock->sending_buf + sock->window.wnd_send->nextseq - 1, len);
              // 正常发送数据，先让adv_window置0吧，其实1也可以，应该没有必要
              // buf + nextseq - 1!  一定要减一！

    sock->window.wnd_send->nextseq += plen - DEFAULT_HEADER_LEN;// 更新nextseq,为下一次判断是否满足流量控制 以及序号做准备。
    // 每次更新只加上新的数据部分的长度，头部不算！！！

    sendToLayer3(msg, plen);
    
    //              发送缓冲区malloc的空间需要释放。暂时考虑释放已经ACK过的数据，放在receive函数中，收到ACK之后清除掉对应的分配空间。
    //               之后再考虑窗口的事
    // 不对，缓存中的空间需要等到ACK之后再释放，这个空间应该不需要等待吧
    // free(data);data =NULL;

    // 互斥锁没有实现，待实现。




    // char* data = malloc(len);
    // memcpy(data, buffer, len);

    // char* msg;
    // uint32_t seq = 464;
    // uint32_t ack = 0;
    // uint16_t plen = DEFAULT_HEADER_LEN + len;
    // uint16_t adv_win = TCP_RECVWN_SIZE;

    // msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
    //         seq, ack, DEFAULT_HEADER_LEN, plen, NO_FLAG, adv_win, 0, data, len);
    // sendToLayer3(msg, plen);

    // free(data);

    printf("发送数据成功！send结束\n");
    return 0;
}


/*接收数据
len是包体长度(不包包头)，buffer是待发送消息的起始指针*/
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    printf("开始recv\n");

    // 没有消息就阻塞
    while(sock->received_len <= 0);

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
    //printf("加锁\n");

    int read_len = 0; // 实际读取的长度
    /*从中读取len长度的数据
    lem: 要读取的数据长度
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


    // 如果在SYN_SENT状态下收到SYN,ACK（三次握手）
    if (sock->state==SYN_SENT && pkt_flags==SYN_FLAG_MASK+ACK_FLAG_MASK) {
        printf("处于SYN_SENT状态下，收到SYN,ACK\n");
        printf("第二次握手完成！\n");

        // 发送ACK
        send_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 改变状态
        sock->state = ESTABLISHED;
        printf("进入ESTABLISHED状态！\n");

        printf("第三次握手完成！\n");
    }


    // 如果在SYN_RECV状态下收到ACK（三次握手）
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




    // 接收方收到数据包
    if(pkt_flags==NO_FLAG) {
        //sock->window.wnd_recv->received[]     sock->window.wnd_recv->expect_seq       TCP_RECVWN_SIZE - sock->received_len    TCP_RECVWN_SIZE
        if(pkt_seq <= sock->window.wnd_recv->expect_seq + TCP_RECVWN_SIZE - sock->received_len) // 如果收到的包在窗口内
        {
            if(pkt_seq == sock->window.wnd_recv->expect_seq)// 如果是期望按序收到的包
            {
                // 把收到的数据放到接收缓冲区 
                while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

                if(sock->received_buf == NULL){// 初始化的时候received_buf是一个空指针。
                    sock->received_buf =(char*) malloc(data_len);
                }else {
                    sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);//重新分配缓冲区的大小
                }
                memcpy(sock->received_buf + sock->received_len , pkt + DEFAULT_HEADER_LEN  , data_len);// 把包体（不包含头部）放进接收缓冲区
                sock->received_len += data_len;
                sock->window.wnd_recv->expect_seq += data_len;
                printf("expect_seq更新为%d\n",sock->window.wnd_recv->expect_seq);
                pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

                printf("数据包按顺序到达，expect_seq=%d, pkt_seq=%d,data已放入接收缓冲区。\n",sock->window.wnd_recv->expect_seq,pkt_seq);
                // printf("接收到的数据包为：%s\n",sock->received_buf + sock->received_len);

                // 检查received[]中的元素是否可以取出，可以取出就取出来
                while(sock->window.wnd_recv->received[1]!='\0')//     如果可以取出。received[]未初始化好像是为全\0，需要try实验一下。
                {
                    //取出received[1]到received_buf中
                    sock->received_buf = realloc(sock->received_buf, sock->received_len + 1);//重新分配缓冲区的大小
                    memcpy(sock->received_buf + sock->received_len, sock->window.wnd_recv->received + 1, 1);// 把包体（不包含头部）放进接收缓冲区
                    sock->received_len += 1;
                    sock->window.wnd_recv->expect_seq ++;

                    // received[]整体左移
                    for(int i = 1 ; i < TCP_RECVWN_SIZE -1 ; i++ )
                    {
                        sock->window.wnd_recv->received[i] = sock->window.wnd_recv->received[i+1];
                    }
                    sock->window.wnd_recv->received[TCP_RECVWN_SIZE - 1] = '\0';
                }
            }
            else    // 否则放到sock->window.wnd_recv->received[]中，作为接收乱序包的缓冲区
            {
                printf("是乱序到达的包，expect_seq=%d, pkt_seq=%d,存入received[]中\n",sock->window.wnd_recv->expect_seq,pkt_seq);
                
                int index;
                for(int i = 0 ; i < data_len ; i++ )
                {
                    index = pkt_seq - sock->window.wnd_recv->expect_seq + i;
                    sock->window.wnd_recv->received[index] = *(pkt + DEFAULT_HEADER_LEN + i);// 这里确实应该加*，把指针赋值给数组的元素没有意义。
                }
            }
            // 发送ACK
            
            printf("接收方发送了ACK！\n");
            
            char* msg;  
            //adv_window = TCP_RECVWN_SIZE - sock->received_len
            msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 0, sock->window.wnd_recv->expect_seq, 
                DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK , TCP_RECVWN_SIZE - sock->received_len , 0, NULL, 0);// 64表示01000000，ACK置位。
            sendToLayer3(msg, DEFAULT_HEADER_LEN);
        }
        else//不在窗口中，忽略掉。
        {
            return 0;
        }
    }
    
    // 发送方收正常的ACK      提取出adv_window : pkt_adv_win，更新rwnd；滑动发送窗口
    // 暂时没有考虑丢包重传
    if(pkt_flags == ACK_FLAG_MASK)
    {
        printf("发送方收到了ACK！\n");
        
        //更新rwnd
        sock->window.wnd_send->rwnd = pkt_adv_win;
        // printf("窗口值更新为%d\n",sock->window.wnd_send->rwnd );
        
        if(pkt_ack == sock->window.wnd_send->base) //如果是按序ACK
        {
            sock->window.wnd_send->base += pkt_plen - pkt_hlen;
        }
        else
        {
            //重传。            RDT再写
        }
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


