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

    sock->window.wnd_recv = NULL;
    sock->window.wnd_recv = NULL;

    return sock;
}


/*
绑定监听的地址
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    // 检测端口是否被占用
    int bind_flag = 0; //端口被占用
    for (int i = 0; i < MAX_SOCK; i++) {
        if (bind_socks[i]->port == bind_addr.port) {
            bind_flag = 1;
            break;
        }
    }

    // 如果被占用
    if (bind_flag) {
        perror("ERROR: 端口被占用！\n");
        exit(-1);
    }
    // 如果未被占用
    else {
        sock->bind_addr = bind_addr;
        // 将socket, port放入b_hash
        int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
        bind_socks[hashval]->port = sock->bind_addr.port;
        bind_socks[hashval]->sock = sock;
    } 
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

    // 循环监听
    while (TRUE) {
        // 创建一个new_socket
        tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));

        //初始化new_conn
        memcpy(new_conn, sock, sizeof(tju_tcp_t));
        
        // 阻塞等待接收SYN（状态改变在handle_packet里）
        while (sock->state != SYN_RECV);
        
        new_conn->state = SYN_RECV;
        sock->state = LISTEN;

        // 设置new_conn对方地址
        tju_sock_addr remote_addr;
        remote_addr.ip = inet_network("10.0.0.2");  //具体的IP地址
        remote_addr.port = 5678;  //从SYN包头获取......
        new_conn->established_remote_addr = remote_addr;

        // 设置new_conn本机地址
        tju_sock_addr local_addr;
        local_addr.ip = sock->bind_addr.ip;  //具体的IP地址
        uint16_t port_tmp = generate_port(); // 随机分配一个port
        // 检测端口是否被占用
        int bind_flag = 0; //端口被占用
        for (int i = 0; i < MAX_SOCK; i++) {
            if (bind_socks[i]->port == port_tmp) {
                bind_flag = 1;
                break;
            }
        }
        // 如果被占用
        if (bind_flag) {
            perror("ERROR: 端口被占用！\n");
            exit(-1);
        }
        // 如果未被占用
        else {
            local_addr.port = port_tmp;
            // 将socket, port放入b_hash
            int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
            bind_socks[hashval]->port = local_addr.port;
            bind_socks[hashval]->sock = new_conn;
        }
        new_conn->established_local_addr = local_addr;

        // 将new_conn放到e_hash和半连接列表中
        int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
        established_socks[hashval] = new_conn;
        semi_conn_socks[hashval] = new_conn;

        // 发送SYN,ACK
        // RFC 793指出ISN可看作是一个32比特的计数器，每4ms加1
        uint32_t isn_server = 1; // 服务器初始序列号
        uint32_t ack = 0; //客户端的ack+1......
        uint16_t plen = DEFAULT_HEADER_LEN;
        uint8_t flags = cal_flags(0, 1, 0, 0, 1, 0, 0, 0);
        char* msg = create_packet_buf(new_conn->established_local_addr.port, new_conn->established_remote_addr.port, 
                isn_server, ack, DEFAULT_HEADER_LEN, plen, flags, 1, 0, NULL, 0);
        sendToLayer3(msg, DEFAULT_HEADER_LEN);

        // 阻塞等待进入ESTABLISHED
        while (sock->state != ESTABLISHED);
        //状态改变在handle_packet里......

        // 将new_conn从半连接列表取出，放到全连接列表
        semi_conn_socks[hashval] = NULL;
        full_conn_socks[hashval] = new_conn;
    }

    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
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

    // 助教写的：如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    return new_conn;
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){
    // 设置对方地址
    sock->established_remote_addr = target_addr;

    // 设置本地地址
    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    uint16_t rand_num = 5678; // 这里应该用rand(time(0))来取某范围的随机数
    // 检测在不在b_hash中......
    local_addr.port = rand_num;
    sock->established_local_addr = local_addr;

    // 发送SYN
    // RFC 793 [Postel 1981c]指出I S N可看作是一个32比特的计数器，每4ms加1
    uint32_t seq = 1; // 客户端初始序列号
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint8_t flags = cal_flags(0, 0, 0, 0, 1, 0, 0, 0);
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, flags, 1, 0, NULL, 0);
    sendToLayer3(msg, DEFAULT_HEADER_LEN);

    // 进入SYN_SENT状态
    sock->state = SYN_SENT;

    // 将socket放入e_hash
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;

    // 阻塞等待进入ESTABLISHED状态
    while (sock->state != ESTABLISHED);
    // 状态的改变在tju_handle_packet

    return 0;
}


/*
发送数据
len是数据长度，不包含包头
*/
int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = 464;
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

    sendToLayer3(msg, plen);
    
    return 0;
}


/*
接收数据
len是数据长度，不包含包头
*/
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len<=0){
        // 阻塞
    }

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0; //实际读取的长度
    /*从中读取len长度的数据
    lem: 要读取的数据长度*/
    if (sock->received_len >= len){ //不能从缓冲区一次读取
        read_len = len;
    }else{
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

    return 0;
}



/*
处理数据包
*/
int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    // 获取报文头部信息
    uint16_t src = get_src(pkt);
    uint16_t dst = get_dst(pkt);
    uint32_t seq = get_seq(pkt);
    uint32_t ack = get_ack(pkt);
    uint16_t hlen = get_hlen(pkt);
    uint16_t plen = get_plen(pkt);
    uint8_t flags = get_flags(pkt);
    uint16_t adv_win = get_advertised_window(pkt);


    // 如果在listen状态下收到SYN
    if (sock->state==LISTEN && flags=8) {
        sock->state = SYN_RECV;
    }

    // 把收到的数据（包体）放到接受缓冲区
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;//包体长度
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL) { // 如果接收缓冲区未初始化
        sock->received_buf = malloc(data_len);
    }
    else {// 如果接收缓冲区已经初始化
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
    return 0;
}

// 随机分配一个port
uint16_t generate_port() {
    srand((unsigned int)time(NULL));
    uint16_t port = rand()%10001 + 3333; //3333 - 13333
    return port;
}