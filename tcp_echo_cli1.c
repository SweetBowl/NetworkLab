//
//  main.c
//  tcp_echo_cl1
//
//  Created by 赵旭 on 2020/6/14.
/*
    TCP ECHO单进程客户端参考模版
    @2020 电子科技大学 信息与软件工程学院 《计算机网络系统》课程组
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#define MAX_CMD_STR 100

void sig_pipe(int signo){
    printf("[cli] SIGPIPE is coming!\n");
}

// 业务逻辑处理函数，定义echo_rqt函数，实现从键盘读入字符串以及发送给服务器，并标准输出
int echo_rqt(int sockfd) {
    char buf[MAX_CMD_STR+1];
    // 从stdin读取1行
    while (fgets(buf, MAX_CMD_STR, stdin)) {
        // TODO 收到exit，退出循环返回
        if (strncmp(buf, "exit", 4)==0) {
            break;
        }
        // TODO 查询所读取1行字符的长度，并将行末'\n'更换为'\0'
        /* int型长度变量在读写时请特别注意字节序转换！强烈建议做如下定义，以示区分：*/
//        int len_h = 0; // 按主机字节序读写的长度变量；
        int len_n = 0; // 按网络字节序读写的长度变量；
            
        int len_h = strnlen(buf, MAX_CMD_STR);    //取字符串长度
        len_n = htonl(len_h);
//        int read_amount = 0,len_to_read=len;
        
        if (buf[len_h-1]=='\n') {
            buf[len_h-1]='\0';
        }
        // TODO 根据读写边界定义，先发数据长度，再发缓存数据
        write(sockfd, &len_n, sizeof(len_n));
        write(sockfd, buf, len_h*sizeof(char));
        memset(buf, 0, MAX_CMD_STR+1);        //缓冲区清零
        // TODO 读取服务器echo回显数据，并打印输出到stdout，依然是先读长度，再根据长度读取数据。
        int res = read(sockfd, &len_n, sizeof(len_n));
        if (res<=0) {
            break;
        }
        len_h = ntohl(len_n);
        res = read(sockfd, buf, len_h);
        while (res<len_h) {
            res = res+read(sockfd,buf+res,len_h-res);
        }
        printf("[echo_rep] %s\n",buf);

/* 在通过read()读取数据时，有可能因为网络传输等问题，使得read()期望读取长度为LEN的数据，但是首次读取仅返回了长度为RES(RES < LEN)的数据（剩余数据尚未接收至系统内核缓存）。因此在read()后必须进行合理判断、循环读取，直至多次read()返回的RES累加和等于LEN，否则读取数据不完整。测试平台刻意制造了必须多次读取的场景，故要求客户端、服务器都必须执行*/
        
    }
    return 0;
}

int main(int argc,char* argv[])
{
    // TODO 定义服务器Socket地址srv_addr；
    struct sockaddr_in srv_addr;
    // TODO 定义Socket连接描述符connfd；
    int connfd;
    // 基于argc简单判断命令行指令输入是否正确；
    if(argc != 3){
        printf("Usage:%s <IP> <PORT>\n", argv[0]);
        return 0;
    }
    
    bzero(&srv_addr,sizeof(srv_addr));
    struct sigaction sigact_pipe, old_sigact_pipe;
    sigact_pipe.sa_handler = sig_pipe;
    sigemptyset(&sigact_pipe.sa_mask);
    sigact_pipe.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);
    
    // TODO 获取Socket连接描述符: connfd = socket(x,x,x);
    if ((connfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Create socket failed.");
        exit(1);
    }
    memset(&srv_addr, 0, sizeof(srv_addr));
    
    // TODO 初始化服务器Socket地址srv_addr，其中会用到argv[1]、argv[2]
        /* IP地址转换推荐使用inet_pton()；端口地址转换推荐使用atoi(); */
    srv_addr.sin_family = AF_INET;
//    inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);
    if(inet_pton(AF_INET, argv[1], &srv_addr.sin_addr) == 0){
        perror("Server IP Address Error:\n");
        exit(1);
    }
    srv_addr.sin_port = htons(atoi(argv[2]));
    
    // TODO 连接服务器，结果存于res: int res = connect(x,x,x);
    do {
        //死循环
        int res =connect(connfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        if (res ==-1) {
            perror("connect failed.");
            exit(1);
        }
        if(res == 0){
            // TODO 连接成功，按题设要求打印服务器端地址server[ip:port]
            char ip_str[20] = {0};
            printf("[cli] server[%s:%d] is connected!\n",inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str)),ntohs(srv_addr.sin_port));
            //如果是非0值，表示程序是非正常退出，终端
            if (!echo_rqt(connfd)) {
                break;
            }
        }
        else if(res == -1 && errno == EINTR){
            printf("[cli] connect error! errno is %d\n", errno);
            continue;
        }
    } while (1);
    // TODO 关闭connfd
    close(connfd);
    printf("[cli] connfd is closed!\n");
    printf("[cli] client is exiting!\n");
    return 0;
}

