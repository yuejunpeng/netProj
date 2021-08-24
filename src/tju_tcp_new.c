#include "tju_tcp.h"

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
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
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    sock->window.wnd_recv = NULL;
    sock->window.wnd_recv = NULL;

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock;
    return 0;
}

/*
接受连接 , 返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
*/
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));

    tju_sock_addr local_addr, remote_addr;

    /*
     这里涉及到TCP连接的建立
     正常来说应该是:
     收到客户端发来的SYN报文
     从中拿到对端的IP和PORT
     把new_conn放到LISTEN的socket的半连接队列和ehash中
     三次握手完成后
     根据发来的ACK的五元组找到这个new_conn
     并将其从半连接队列取出 放到全连接队列中
     换句话说 下面的处理流程其实不应该放在这里 应该在tju_handle_packet中
    */ 
    remote_addr.ip = inet_network("10.0.0.2");  //具体的IP地址
    remote_addr.port = 5678;  //端口

    local_addr.ip = listen_sock->bind_addr.ip;  //具体的IP地址
    local_addr.port = listen_sock->bind_addr.port;  //端口

    new_conn->established_local_addr = local_addr;
    new_conn->established_remote_addr = remote_addr;

    // 这里应该是经过三次握手后才能修改状态为ESTABLISHED
    new_conn->state = ESTABLISHED;

    // 将新的conn放到内核建立连接的socket哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
    established_socks[hashval] = new_conn;

    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 




    // 伪代码
    // 接收客户端发来的syn
    char buf[2021];
    tju_recv(listen_sock, (void*)buf, 20);

    // 如果syn位为1
    uint8_t flags =  get_flags(buf);
    if (flags = 7) {
        // 新建一个socket
        tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));

        // 设置ip和port
        tju_sock_addr local_addr, remote_addr;
        remote_addr.ip = inet_network("10.0.0.2");  //具体的IP地址
        remote_addr.port = get_src(buf);
         local_addr.ip = listen_sock->bind_addr.ip;
         local_addr.port = listen_sock->bind_addr.port;
         new_conn->established_local_addr = local_addr;
         new_conn->established_remote_addr = remote_addr;

         // 初始化new_conn
         pthread_mutex_init(&(sock->send_lock), NULL);
         sock->sending_buf = NULL;
         sock->sending_len = 0;
         
         pthread_mutex_init(&(sock->recv_lock), NULL);
         sock->received_buf = NULL;
         sock->received_len = 0;
         
         if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
             perror("ERROR condition variable not set\n");
             exit(-1);
             }
        
        sock->window.wnd_recv = NULL;
        sock->window.wnd_recv = NULL;

        // 将新的conn放到ehash和半连接列表中
        int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
        established_socks[hashval] = new_conn;
        semi_conn_socks[hashval] = new_conn;

        // 发送SYN，ACK，进入SYN_RECV状态
        tju_send(syn);
        tju_send(ack);
        sock->state = SYN_RECV;
    }
    


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
    // 确定源地址和目的地址
    sock->established_remote_addr = target_addr;
    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    local_addr.port = generate_random_port; // 随机分配一个可用的port（伪）
    sock->established_local_addr = local_addr;

    // 将socket放入bhash中
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    bind_socks[hashval]->sock = sock;
    bind_socks[hashval]->port = sock->established_local_addr.port;

    // 第一次握手
    tju_send(sock, "test", 5, syn == 1);// 发送一个SYN给服务器（伪）
    sock->state = SYN_SENT;

    // 将socket放入ehash
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;

    // 阻塞等待建立连接
    while(sock->state != ESTABLISHED);
    // socket状态的改变来自收到服务器的SYNACK，这是在tju_handle_packet中处理的
    next_process_process; // 建立连接后的一些处理过程（伪）
    
    return 0;
}


/*
发送数据
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
*/
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len<=0){
        // 阻塞
    }

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0;
    if (sock->received_len >= len){ // 从中读取len长度的数据
        read_len = len;
    }else{
        read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
    }

    // 从存储区 str2 复制 n 个字节到存储区 str1
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
    
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

    // 把收到的数据放到接受缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL){
        sock->received_buf = malloc(data_len);
    }else {
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







//第三次握手
client:
// 收到syn和ack后，发送ack，修改socket的状态
after(recv syn & ack){
    tju_send(ack);
    socket->state = ESTABLISHED;
}

server:
// 收到ack后，修改socket的状态，new_conn放入全连接队列
after(recv ack){
    socket->state = ESTABLISHED;
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    full_conn_socks[hashval] = new_conn;
}
