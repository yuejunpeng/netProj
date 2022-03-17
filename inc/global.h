#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "global.h"
#include <pthread.h>
#include <sys/select.h>
#include <arpa/inet.h>

// 单位是byte
#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

// 一些Flag，应该可以再加一些吧
#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2
#define TRUE 1
#define FALSE 0

// 定义最大包长 防止IP层分片
#define MAX_DLEN 1375 	// 最大包内数据长度
#define MAX_LEN 1400 	// 最大包长度

// TCP socket 状态定义
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECV 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10

#define a 0.125
#define b 0.25

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// 数据包
typedef struct {
	char* data; // 数据起始地址
	uint16_t data_len; // 数据包长度
	uint32_t seq; // 字节序号

}pkt_data;

// 数据包_发送
typedef struct {
	char* msg; // 数据包（头部+包体）起始地址
	uint16_t plen; // 数据包长度
	uint16_t data_len; // 数据部分长度

}pkt_data_send;

// TCP 接受窗口大小
#define TCP_RECVWN_SIZE 32*MAX_DLEN // 比如最多放32个满载数据包

// TCP 发送窗口
typedef struct {
	pkt_data_send* resend_buf[TCP_RECVWN_SIZE]; // 重传包缓冲区, 索引是 seq - base，从0开始，每次收到ACK要整体左移。

	// uint16_t window_size; // 窗口大小
	uint32_t base; // base = LastByteAcked + 1
	uint32_t nextseq; // 主要用来当作序号seq, nextseq = LastByteSent + 1
	double ertt;// 待初始化
	double rttvar ;// 待初始化；
	double rto ; // 待初始化；
	int ack_cnt; // 应该是三次冗余ACK使用
	pthread_mutex_t ack_cnt_lock;
	// struct timeval send_time;
	// struct timeval timeout;
	uint16_t rwnd; // 接收窗口大小
	// int congestion_status;
	// uint16_t cwnd; 
	// uint16_t ssthresh; 
} sender_window_t;

// TCP 接收窗口
typedef struct {
	pkt_data* disorder_buf[TCP_RECVWN_SIZE]; // 乱序包缓冲区, 索引是pkt_seq - expect_seq。
	// 由于expect_seq会一直更新，所以需要每收到一个正确的包就更新一次。

	// received_packet_t* head;
	// char buf[TCP_RECVWN_SIZE];
	// uint8_t marked[TCP_RECVWN_SIZE];
	uint32_t expect_seq; // 期望的确认号，每次receive时更新

} receiver_window_t;

// TCP 窗口 每个建立了连接的TCP都包括发送和接受两个窗口
typedef struct {
	sender_window_t* wnd_send;// 发送窗口
  	receiver_window_t* wnd_recv;// 接收窗口
} window_t;

// 地址
typedef struct {
	uint32_t ip;
	uint16_t port;
} tju_sock_addr;

// 计时器，使用全局变量实现一个计时器。	新加部分
typedef struct{
	double start_time,now_time; // start_time表示开启计时器的时刻，now_time表示当前时刻，用来判断是否超时。
	double timer_maxtime;// 设定的RTO，待初始化为0
	int timer_istimeout;// 是否timeout，待初始化为0
	int quit_timer; // 计时器是否在运行，1 表示在运行，0 表示关闭。待初始化为0
	
	// tju_tcp_t * sock;// 是哪个socket的timer。
	
	// tju_sock_addr established_local_addr; // 建立连接后本机的IP和端口
	// tju_sock_addr established_remote_addr; // 建立连接后对方的IP和端口

	uint32_t seq;// 是发送哪个包的时候开启的计时器。
	pthread_mutex_t timer_seq_lock;	// 获取seq可能会出现竞态条件，要加锁。
}timer_t_my;

// TCP 结构体（socket）
typedef struct {
	int state; // TCP的状态

	tju_sock_addr bind_addr; // bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 建立连接后本机的IP和端口
	tju_sock_addr established_remote_addr; // 建立连接后对方的IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	char* sending_buf; // 发送数据缓存区
	int sending_len; // 发送数据缓存长度

	pthread_mutex_t recv_lock; // 接收数据锁
	char* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度

	pthread_mutex_t received_len_lock;

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	window_t window; // 发送和接受窗口

	timer_t_my timer;// 计时器
	
} tju_tcp_t;




#endif