#include "tju_tcp.h"

/*
创建并初始化 TCP socket
*/
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

    sock->window.wnd_send = NULL;
    sock->window.wnd_recv = NULL;

    printf("socket创建完成！\n");

    return sock;
}


/*
绑定监听的地址
*/
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


/*
被动打开，监听bind的地址和端口
*/
int tju_listen(tju_tcp_t* sock){
    // 进入listen状态
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock; //注册该socket到l_hash
    printf("listen完成！\n");
    return 0;
}


/*
接受连接 
返回与客户端通信用的socket
*/
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


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
*/
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

    sleep(3);

    printf("connect完成！\n");
    
    return 0;
}


/*
发送数据
len是数据长度，不包含包头
*/
int tju_send(tju_tcp_t* sock, const void *buffer, int len){

    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = 464;
    uint32_t ack = 0;
    uint16_t plen = DEFAULT_HEADER_LEN + len;
    uint16_t adv_win = TCP_RECVWN_SIZE;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, NO_FLAG, adv_win, 0, data, len);
    sendToLayer3(msg, plen);

    free(data);

    printf("发送数据成功！\n");
    return 0;
}


/*
接收数据
len是数据长度，不包含包头
*/
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len <= 0){
        // 阻塞
    }

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0; //实际读取的长度
    /*从中读取len长度的数据
    lem: 要读取的数据长度*/
    if (sock->received_len >= len){ //不能从缓冲区一次读取
        read_len = len;
    }
    else{
        read_len = sock->received_len; // 从缓冲区读取所有数据
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len);
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
        free(sock->received_buf);
        sock->received_len -= read_len;
        sock->received_buf = new_buf;
    }else{
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    printf("接收数据成功！\n");

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

    // 如果在LISTEN状态下收到SYN（三次握手）
    if (sock->state==LISTEN && pkt_flags==cal_flags(0, 0, 0, 0, 1, 0, 0, 0)) {
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
        printf("new_conn被放入e_hash和半连接列表\n");

        // 发送SYN,ACK
        send_syn_ack(new_conn, 1, pkt_ack+1, TCP_RECVWN_SIZE);

        // 更改new_conn状态为SYN_RECV
        new_conn->state = SYN_RECV;
        printf("进入SYN_RECV状态\n");

        printf("第二次握手完成！\n");
    }


    // 如果在SYN_SENT状态下收到SYN,ACK（三次握手）
    if (sock->state==SYN_SENT && pkt_flags==cal_flags(0, 1, 0, 0, 1, 0, 0, 0)) {
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
    if (sock->state==SYN_RECV && pkt_flags==cal_flags(0, 1, 0, 0, 0, 0, 0, 0)) {
        printf("处于SYN_RECV状态下，收到ACK\n");
        
        // 将new_conn从半连接列表取出，放到全连接列表
        int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
                sock->established_remote_addr.ip, sock->established_remote_addr.port);
        semi_conn_socks[hashval] = NULL;
        full_conn_socks[hashval] = sock;
        printf("new_conn从半连接列表取出，被放到全连接列表\n");

        sock->state = ESTABLISHED;
        printf("进入ESTABLISHED状态\n");

        printf("第三次握手完成！\n");
    }

    
    // 如果在SYN_SENT状态下收到SYN（同时打开）
    if (sock->state==SYN_SENT && pkt_flags==cal_flags(0, 0, 0, 0, 1, 0, 0, 0)) {
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
    if (sock->state==ESTABLISHED && pkt_flags==cal_flags(0, 0, 0, 0, 0, 1, 0, 0)) {
        printf("处于ESTABLISHED状态下，收到FIN\n");
        printf("第一次挥手完成\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入CLOSE_WAIT状态
        sock->state = CLOSE_WAIT;
        printf("进入CLOSE_WAIT状态\n");

        printf("第二次挥手完成\n");

        // 等待数据发送完
        sleep(5); // 这里要研究研究
        printf("等待数据发送完\n");

        // 发送FIN
        send_fin(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入LAST_ACK
        sock->state = LAST_ACK;
        printf("进入LAST_ACK状态\n");

        printf("第三次挥手完成！\n");
    }


    // 如果在FIN_WAIT_1状态收到FIN,ACK（四次挥手）
    if (sock->state==FIN_WAIT_1 && pkt_flags==cal_flags(0, 1, 0, 0, 0, 1, 0, 0)) {
        printf("处于FIN_WAIT_1状态下，收到FIN,ACK\n");

        sock->state = FIN_WAIT_2;

        printf("进入FIN_WAIT_2\n");
        printf("第二次挥手完成\n");
    }


    // 如果在FIN_WAIT_2状态收到FIN（四次挥手）
    if (sock->state==FIN_WAIT_2 && pkt_flags==cal_flags(0, 0, 0, 0, 0, 1, 0, 0)) {
        printf("处于FIN_WAIT_2状态下，收到FIN\n");
        printf("第三次挥手完成\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入TIME_WAIT状态
        sock->state = TIME_WAIT;
        printf("进入TIME_WAIT状态\n");

        printf("第四次挥手完成\n");

        // 等待2MSS后关闭
        sleep(5); // 还需进一步研究
        sock->state = CLOSED;

        printf("等待2MSS后，进入CLOSED状态\n");
    }


    // 如果在LAST_ACK状态收到FIN,ACK（四次挥手）
    if (sock->state==LAST_ACK && pkt_flags==cal_flags(0, 1, 0, 0, 0, 1, 0, 0)) {
        printf("处于LAST_ACK状态下，收到FIN,ACK\n");

        sock->state = CLOSED;

        printf("进入CLOSED状态\n");
        printf("第四次挥手完成\n");
    }


    // 如果在FIN_WAIT_1状态收到FIN（同时关闭）
    if (sock->state==FIN_WAIT_1 && pkt_flags==cal_flags(0, 0, 0, 0, 0, 1, 0, 0)) {
        printf("处于FIN_WAIT_1状态下，收到FIN（同时关闭）\n");

        // 发送FIN,ACK
        send_fin_ack(sock, pkt_ack, pkt_seq+1, TCP_RECVWN_SIZE);

        // 进入CLOSING状态
        sock->state = CLOSING;
        printf("进入CLOSING\n");
    }


    // 如果在CLOSING状态收到FIN,ACK（同时关闭）
    if (sock->state==CLOSING && pkt_flags==cal_flags(0, 1, 0, 0, 0, 1, 0, 0)) {
        printf("处于CLOSING状态下，收到FIN,ACK（同时关闭）\n");

        // 进入TIME_WAIT状态
        sock->state = TIME_WAIT;
        printf("进入TIME_WAIT状态\n");

        // 等待2MSS后关闭
        sleep(5); // 还需进一步研究
        sock->state = CLOSED;
        printf("等待2MSS后，进入CLOSED状态\n");
    }



    // 把收到的数据（包体）放到接受缓冲区
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;//包体长度

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
    // 如果接收缓冲区未初始化
    if(sock->received_buf == NULL) { 
        sock->received_buf = malloc(data_len);
    }
    // 如果接收缓冲区已经初始化
    else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

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
    uint8_t flags = cal_flags(0, 0, 0, 0, 1, 0, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port,
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个SYN！\n");
}


// 发送SYN,ACK
void send_syn_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = cal_flags(0, 1, 0, 0, 1, 0, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个SYN,ACK！\n");
}


// 发送FIN
void send_fin(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = cal_flags(0, 0, 0, 0, 0, 1, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个FIN！\n");
}


// 发送FIN,ACK
void send_fin_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = cal_flags(0, 1, 0, 0, 0, 1, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个FIN,ACK！\n");
}


// 发送ACK
void send_ack(tju_tcp_t* sock, uint32_t seq, uint32_t ack, uint16_t adv_win) {
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = cal_flags(0, 1, 0, 0, 0, 0, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 
            seq, ack, DEFAULT_HEADER_LEN, plen, flags, adv_win, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);
    printf("已经发送一个ACK！\n");
}


