#include "stdio.h"

int main() {
    printf();
}


    // 分配端口
    while (TRUE) {
        uint16_t port_tmp = generate_port(); // 随机分配一个port
        int hashval = cal_hash(local_addr.ip, port_tmp, target_addr.ip, target_addr.port);

        // 检测端口是否被占用
        int bind_flag = 0; //端口被占用
        for (int i = 0; i < MAX_SOCK; i++) {
            if (bind_socks[i]->port == port_tmp) {
                bind_flag = 1;
                break;
            }
        }

        // 如果未被占用
        if (!bind_flag) {
            local_addr.port = port_tmp;

            // 存入b_hash
            bind_socks[hashval]->port = port_tmp;
            bind_socks[hashval].sock = (tju_tcp_t*) malloc(sizeof(tju_tcp_t));
            bind_socks[hashval]->sock = sock;
            printf("分配端口成功!\n");
            break;
        }

        // 如果被占用
        if (bind_flag) {
            printf("端口被占用，重新分配端口！\n");
        }
    }



// 将socket放入e_hash
int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
        sock->established_remote_addr.ip, sock->established_remote_addr.port);
established_socks[hashval] = sock;