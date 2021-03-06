#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "global.h"
#include "tju_packet.h"
#include <unistd.h>
#include "tju_tcp.h"

#define MAX_SOCK 32

// sock, port键值对
typedef struct {
	tju_tcp_t* sock;
  uint16_t port;
}tju_tcp_port_t;

// 3张hash表
tju_tcp_t* listen_socks[MAX_SOCK];
tju_tcp_t* established_socks[MAX_SOCK];
tju_tcp_port_t* bind_socks[MAX_SOCK];

// 2个连接列表
tju_tcp_t* semi_conn_socks[MAX_SOCK];
tju_tcp_t* full_conn_socks[MAX_SOCK];

/*
模拟Linux内核收到一份TCP报文的处理函数
*/
void onTCPPocket(char* pkt);


/*
以用户填写的TCP报文为参数
根据用户填写的TCP的目的IP和目的端口,向该地址发送数据报
*/
void sendToLayer3(char* packet_buf, int packet_len);


/*
开启仿真, 运行起后台线程
*/
void startSimulation();


/*
 使用UDP进行数据接收的线程
*/
void* receive_thread(void * in);


// 计时器线程，始终开启，判断是否超时等
void* timer_my_thread(void* arg);

// 开启（重启）计时器，传入当前的RTO。
void reset_my_timer(double maxtime,tju_tcp_t* sock,uint32_t seq);

// 强制重启计时器
void reset_my_timer_f(double maxtime,tju_tcp_t* sock,uint32_t seq);


// 接受UDP的socket的标识符
int BACKEND_UDPSOCKET_ID;


/*
 linux内核会根据
 本地IP 本地PORT 远端IP 远端PORT 计算hash值 四元组 
 找到唯一的那个socket

 (实际上真正区分socket的是五元组
  还有一个协议字段
  不过由于本项目是TCP 协议都一样, 就没必要了)
*/
int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);

#endif